// Protocol Buffers - Google's data interchange format
// Copyright 2008 Google Inc.  All rights reserved.
// https://developers.google.com/protocol-buffers/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef GOOGLE_PROTOBUF_JSON_INTERNAL_PARSER_TRAITS_H__
#define GOOGLE_PROTOBUF_JSON_INTERNAL_PARSER_TRAITS_H__

#include <cfloat>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <utility>

#include "google/protobuf/type.pb.h"
#include "y_absl/base/attributes.h"
#include "y_absl/base/casts.h"
#include "y_absl/container/flat_hash_map.h"
#include "y_absl/container/flat_hash_set.h"
#include "y_absl/status/status.h"
#include "y_absl/strings/str_format.h"
#include "y_absl/strings/string_view.h"
#include "google/protobuf/io/coded_stream.h"
#include "google/protobuf/io/zero_copy_stream.h"
#include "google/protobuf/io/zero_copy_stream_impl_lite.h"
#include "google/protobuf/json/internal/descriptor_traits.h"
#include "google/protobuf/wire_format_lite.h"
#include "google/protobuf/stubs/status_macros.h"

// Must be included last.
#include "google/protobuf/port_def.inc"

namespace google {
namespace protobuf {
namespace json_internal {
using ::google::protobuf::internal::WireFormatLite;

// See the comment in json_util2_parser.cc for more information.
//
// The type traits in this file  describe how to parse to a protobuf
// representation used by the JSON API, either via proto reflection or by
// emitting wire format to an output stream.

// Helper alias templates to avoid needing to write `typename` in function
// signatures.
template <typename Traits>
using Msg = typename Traits::Msg;

struct ParseProto2Descriptor : Proto2Descriptor {
  // A message value that fields can be written to, but not read from.
  class Msg {
   public:
    explicit Msg(Message* msg) : msg_(msg) {}

   private:
    friend ParseProto2Descriptor;
    Message* msg_;
    // Because `msg` might already have oneofs set, we need to track which were
    // set *during* the parse separately.
    y_absl::flat_hash_set<int> parsed_oneofs_indices_;
    y_absl::flat_hash_set<int> parsed_fields_;
  };

  static bool HasParsed(Field f, const Msg& msg,
                        bool allow_repeated_non_oneof) {
    if (f->real_containing_oneof()) {
      return msg.parsed_oneofs_indices_.contains(
          f->real_containing_oneof()->index());
    }
    if (allow_repeated_non_oneof) {
      return false;
    }
    return msg.parsed_fields_.contains(f->number());
  }

  /// Functions for writing fields. ///

  // Marks a field as having been "seen". This will clear the field if it is
  // the first occurrence thereof.
  //
  // All setters call this function automatically, but it may also be called
  // eagerly to clear a pre-existing value that might not be overwritten, such
  // as when parsing a repeated field.
  static void RecordAsSeen(Field f, Msg& msg) {
    bool inserted = msg.parsed_fields_.insert(f->number()).second;
    if (inserted) {
      msg.msg_->GetReflection()->ClearField(msg.msg_, f);
    }

    if (f->real_containing_oneof() != nullptr) {
      msg.parsed_oneofs_indices_.insert(f->real_containing_oneof()->index());
    }
  }

  // Adds a new message and calls body on it.
  //
  // Body should have a signature `y_absl::Status(const Desc&, Msg&)`.
  template <typename F>
  static y_absl::Status NewMsg(Field f, Msg& msg, F body) {
    RecordAsSeen(f, msg);

    Message* new_msg;
    if (f->is_repeated()) {
      new_msg = msg.msg_->GetReflection()->AddMessage(msg.msg_, f);
    } else {
      new_msg = msg.msg_->GetReflection()->MutableMessage(msg.msg_, f);
    }
    Msg wrapper(new_msg);
    return body(*f->message_type(), wrapper);
  }

  // Adds a new dynamic message with the given type name and calls body on it.
  //
  // Body should have a signature `y_absl::Status(const Desc&, Msg&)`.
  template <typename F>
  static y_absl::Status NewDynamic(Field f, const TProtoStringType& type_url, Msg& msg,
                                 F body) {
    RecordAsSeen(f, msg);
    return WithDynamicType(
        *f->containing_type(), type_url, [&](const Desc& desc) -> y_absl::Status {
          DynamicMessageFactory factory;
          std::unique_ptr<Message> dynamic(factory.GetPrototype(&desc)->New());
          Msg wrapper(dynamic.get());
          RETURN_IF_ERROR(body(desc, wrapper));

          if (f->is_repeated()) {
            msg.msg_->GetReflection()->AddString(
                msg.msg_, f, dynamic->SerializePartialAsString());
          } else {
            msg.msg_->GetReflection()->SetString(
                msg.msg_, f, dynamic->SerializePartialAsString());
          }
          return y_absl::OkStatus();
        });
  }

  static void SetFloat(Field f, Msg& msg, float x) {
    RecordAsSeen(f, msg);
    if (f->is_repeated()) {
      msg.msg_->GetReflection()->AddFloat(msg.msg_, f, x);
    } else {
      msg.msg_->GetReflection()->SetFloat(msg.msg_, f, x);
    }
  }

  static void SetDouble(Field f, Msg& msg, double x) {
    if (f->is_repeated()) {
      msg.msg_->GetReflection()->AddDouble(msg.msg_, f, x);
    } else {
      msg.msg_->GetReflection()->SetDouble(msg.msg_, f, x);
    }
  }

  static void SetInt64(Field f, Msg& msg, arc_i64 x) {
    RecordAsSeen(f, msg);
    if (f->is_repeated()) {
      msg.msg_->GetReflection()->AddInt64(msg.msg_, f, x);
    } else {
      msg.msg_->GetReflection()->SetInt64(msg.msg_, f, x);
    }
  }

  static void SetUInt64(Field f, Msg& msg, arc_ui64 x) {
    RecordAsSeen(f, msg);
    if (f->is_repeated()) {
      msg.msg_->GetReflection()->AddUInt64(msg.msg_, f, x);
    } else {
      msg.msg_->GetReflection()->SetUInt64(msg.msg_, f, x);
    }
  }

  static void SetInt32(Field f, Msg& msg, int32 x) {
    RecordAsSeen(f, msg);
    if (f->is_repeated()) {
      msg.msg_->GetReflection()->AddInt32(msg.msg_, f, x);
    } else {
      msg.msg_->GetReflection()->SetInt32(msg.msg_, f, x);
    }
  }

  static void SetUInt32(Field f, Msg& msg, uint32 x) {
    RecordAsSeen(f, msg);
    if (f->is_repeated()) {
      msg.msg_->GetReflection()->AddUInt32(msg.msg_, f, x);
    } else {
      msg.msg_->GetReflection()->SetUInt32(msg.msg_, f, x);
    }
  }

  static void SetBool(Field f, Msg& msg, bool x) {
    RecordAsSeen(f, msg);
    if (f->is_repeated()) {
      msg.msg_->GetReflection()->AddBool(msg.msg_, f, x);
    } else {
      msg.msg_->GetReflection()->SetBool(msg.msg_, f, x);
    }
  }

  static void SetString(Field f, Msg& msg, y_absl::string_view x) {
    RecordAsSeen(f, msg);
    if (f->is_repeated()) {
      msg.msg_->GetReflection()->AddString(msg.msg_, f, TProtoStringType(x));
    } else {
      msg.msg_->GetReflection()->SetString(msg.msg_, f, TProtoStringType(x));
    }
  }

  static void SetEnum(Field f, Msg& msg, arc_i32 x) {
    RecordAsSeen(f, msg);
    if (f->is_repeated()) {
      msg.msg_->GetReflection()->AddEnumValue(msg.msg_, f, x);
    } else {
      msg.msg_->GetReflection()->SetEnumValue(msg.msg_, f, x);
    }
  }
};

// Traits for proto3-ish deserialization.
//
// This includes a rudimentary proto serializer, since message fields are
// written directly instead of being reflectively written to a proto field.
//
// See MessageTraits for API docs.
struct ParseProto3Type : Proto3Type {
  class Msg {
   public:
    explicit Msg(io::ZeroCopyOutputStream* stream) : stream_(stream) {}

   private:
    friend ParseProto3Type;
    io::CodedOutputStream stream_;
    y_absl::flat_hash_set<arc_i32> parsed_oneofs_indices_;
    y_absl::flat_hash_set<arc_i32> parsed_fields_;
  };

  static bool HasParsed(Field f, const Msg& msg,
                        bool allow_repeated_non_oneof) {
    if (f->proto().oneof_index() != 0) {
      return msg.parsed_oneofs_indices_.contains(f->proto().oneof_index());
    }
    if (allow_repeated_non_oneof) {
      return false;
    }
    return msg.parsed_fields_.contains(f->proto().number());
  }

  /// Functions for writing fields. ///

  static void RecordAsSeen(Field f, Msg& msg) {
    msg.parsed_fields_.insert(f->proto().number());
    if (f->proto().oneof_index() != 0) {
      msg.parsed_oneofs_indices_.insert(f->proto().oneof_index());
    }
  }

  template <typename F>
  static y_absl::Status NewMsg(Field f, Msg& msg, F body) {
    return NewDynamic(f, f->proto().type_url(), msg, body);
  }

  template <typename F>
  static y_absl::Status NewDynamic(Field f, const TProtoStringType& type_url, Msg& msg,
                                 F body) {
    RecordAsSeen(f, msg);
    return WithDynamicType(
        f->parent(), type_url, [&](const Desc& desc) -> y_absl::Status {
          if (f->proto().kind() == google::protobuf::Field::TYPE_GROUP) {
            msg.stream_.WriteTag(f->proto().number() << 3 |
                                 WireFormatLite::WIRETYPE_START_GROUP);
            RETURN_IF_ERROR(body(desc, msg));
            msg.stream_.WriteTag(f->proto().number() << 3 |
                                 WireFormatLite::WIRETYPE_END_GROUP);
            return y_absl::OkStatus();
          }

          TProtoStringType out;
          io::StringOutputStream stream(&out);
          Msg new_msg(&stream);
          RETURN_IF_ERROR(body(desc, new_msg));

          new_msg.stream_.Trim();  // Should probably be called "Flush()".
          y_absl::string_view written(
              out.data(), static_cast<size_t>(new_msg.stream_.ByteCount()));
          SetString(f, msg, written);
          return y_absl::OkStatus();
        });
  }

  static void SetFloat(Field f, Msg& msg, float x) {
    RecordAsSeen(f, msg);
    msg.stream_.WriteTag(f->proto().number() << 3 |
                         WireFormatLite::WIRETYPE_FIXED32);
    msg.stream_.WriteLittleEndian32(y_absl::bit_cast<arc_ui32>(x));
  }

  static void SetDouble(Field f, Msg& msg, double x) {
    RecordAsSeen(f, msg);
    msg.stream_.WriteTag(f->proto().number() << 3 |
                         WireFormatLite::WIRETYPE_FIXED64);
    msg.stream_.WriteLittleEndian64(y_absl::bit_cast<arc_ui64>(x));
  }

  static void SetInt64(Field f, Msg& msg, arc_i64 x) {
    SetInt<arc_i64, google::protobuf::Field::TYPE_INT64,
           google::protobuf::Field::TYPE_SFIXED64,
           google::protobuf::Field::TYPE_SINT64>(f, msg, x);
  }

  static void SetUInt64(Field f, Msg& msg, arc_ui64 x) {
    SetInt<arc_ui64, google::protobuf::Field::TYPE_UINT64,
           google::protobuf::Field::TYPE_FIXED64,
           google::protobuf::Field::TYPE_UNKNOWN>(f, msg, x);
  }

  static void SetInt32(Field f, Msg& msg, arc_i32 x) {
    SetInt<arc_i32, google::protobuf::Field::TYPE_INT32,
           google::protobuf::Field::TYPE_SFIXED32,
           google::protobuf::Field::TYPE_SINT32>(f, msg, x);
  }

  static void SetUInt32(Field f, Msg& msg, arc_ui32 x) {
    SetInt<arc_ui32, google::protobuf::Field::TYPE_UINT32,
           google::protobuf::Field::TYPE_FIXED32,
           google::protobuf::Field::TYPE_UNKNOWN>(f, msg, x);
  }

  static void SetBool(Field f, Msg& msg, bool x) {
    RecordAsSeen(f, msg);
    msg.stream_.WriteTag(f->proto().number() << 3);
    char b = x ? 0x01 : 0x00;
    msg.stream_.WriteRaw(&b, 1);
  }

  static void SetString(Field f, Msg& msg, y_absl::string_view x) {
    RecordAsSeen(f, msg);
    msg.stream_.WriteTag(f->proto().number() << 3 |
                         WireFormatLite::WIRETYPE_LENGTH_DELIMITED);
    msg.stream_.WriteVarint64(static_cast<arc_ui64>(x.size()));
    msg.stream_.WriteRaw(x.data(), x.size());
  }

  static void SetEnum(Field f, Msg& msg, arc_i32 x) {
    RecordAsSeen(f, msg);
    msg.stream_.WriteTag(f->proto().number() << 3);
    // Sign extension is deliberate here.
    msg.stream_.WriteVarint32(x);
  }

 private:
  using Kind = google::protobuf::Field::Kind;
  // Sets a field of *some* integer type, with the given kinds for the possible
  // encodings. This avoids quadruplicating this code in the helpers for the
  // four major integer types.
  template <typename Int, Kind varint, Kind fixed, Kind zigzag>
  static void SetInt(Field f, Msg& msg, Int x) {
    RecordAsSeen(f, msg);
    switch (f->proto().kind()) {
      case zigzag:
        // Regardless of the integer type, ZigZag64 will do the right thing,
        // because ZigZag is not dependent on the width of the integer: it is
        // always `2 * abs(n) + (n < 0)`.
        x = static_cast<Int>(
            internal::WireFormatLite::ZigZagEncode64(static_cast<arc_i64>(x)));
        Y_ABSL_FALLTHROUGH_INTENDED;
      case varint:
        msg.stream_.WriteTag(f->proto().number() << 3 |
                             WireFormatLite::WIRETYPE_VARINT);
        if (sizeof(Int) == 4) {
          msg.stream_.WriteVarint32(static_cast<arc_ui32>(x));
        } else {
          msg.stream_.WriteVarint64(static_cast<arc_ui64>(x));
        }
        break;
      case fixed: {
        if (sizeof(Int) == 4) {
          msg.stream_.WriteTag(f->proto().number() << 3 |
                               WireFormatLite::WIRETYPE_FIXED32);
          msg.stream_.WriteLittleEndian32(static_cast<arc_ui32>(x));
        } else {
          msg.stream_.WriteTag(f->proto().number() << 3 |
                               WireFormatLite::WIRETYPE_FIXED64);
          msg.stream_.WriteLittleEndian64(static_cast<arc_ui64>(x));
        }
        break;
      }
      default: {  // Unreachable.
      }
    }
  }
};
}  // namespace json_internal
}  // namespace protobuf
}  // namespace google

#include "google/protobuf/port_undef.inc"
#endif  // GOOGLE_PROTOBUF_JSON_INTERNAL_PARSER_TRAITS_H__
