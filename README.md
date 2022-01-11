# Overview

Introducing...Protobuf SUPER Lite! Instead of declaring protobufs in a separate
`.proto` file, and having to generate the C++ code (and build/install the
`protoc` utility in the first place), and then having to compile the generated
C++ code; let's instead leverage the wonders of the C++ type system to achieve
the same end, with a 90% reduction in developer effort; and also get very
nicely-optimized (de)serialization code from the C++ toolchain's optimizer.

In other words, you provide simple C++ structs/classes that best fit the needs
of your application design, and let this library figure out, at *compile* time,
how those structs will map to equivalent `.proto` constructs, and how they will
serialize/parse into wire-format proto.

This library is a loaded gun, and you can easily shoot yourself in the foot.
With power comes responsibility, after all. Thus, it is well worth your time to
read through this entire document.

This documentation assumes a basic familiarity with Google's Protocol Buffers.
If you are new to this, please first read [Protocol Buffer
Basics](https://developers.google.com/protocol-buffers/docs/cpptutorial).

# License

All documentation and source code is covered by a BSD-style license, found in
the `LICENSE` file in the root directory of this project.

# Usage

C++ classes/structs that should represent protobuf messages declare public data
members, and then add a `ProtobufFields` type alias to provide a mapping between
message field numbers and the data members. That's it! For example:

```
#include "pb/field_list.h"
#include "pb/integer_wrapper.h"  // Support for sint32/64, fixed32/64, etc.

// message AudioConfig {
//   int32 sample_rate = 1;
//   int32 bit_depth = 2;
//   int32 channel_count = 3;
//   sint32 delay_adjustment_ms = 4;
//   fixed64 destination_id = 5;
// }
struct AudioConfig {
  int32_t sample_rate;
  int32_t bit_depth;
  int32_t channel_count;
  pb::sint32_t delay_adjustment_ms;
  pb::fixed64_t destination_id;

  using ProtobufFields = pb::FieldList<
      pb::Field<&AudioConfig::sample_rate, 1>,
      pb::Field<&AudioConfig::bit_depth, 2>,
      pb::Field<&AudioConfig::channel_count, 3>,
      pb::Field<&AudioConfig::delay_adjustment_ms, 4>,
      pb::Field<&AudioConfig::destination_id, 5>>;
};
```

The above demonstrates the usage of several of the [scalar value
types](https://developers.google.com/protocol-buffers/docs/proto3#scalar). All
proto3 scalar types are supported, with `string` and `bytes` both being
represented in C++ code as `std::string`.

To parse messages from a byte array containing wire-format data, simply
`#include "pb/parse.h"` and call `pb::MergeFromBuffer()` or `pb::Parse()`. The
header comments describe when to use each of these functions. For example:

```
#include "audio_config.h"
#include "pb/parse.h"

bool OnReceivedAudioConfig(const uint8_t* buffer, int buffer_size) {
  AudioConfig config{};
  if (!pb::MergeFromBuffer(buffer, buffer + buffer_size, config)) {
    return false;
  }

  // ...Validate |config| data members, take action, etc...

  return true;
}
```

To serialize messages into a byte array, first `#include "pb/serialize.h`. Then,
call `pb::ComputeSerializedSize()` to compute the required size of the byte
array. Allocate the byte array, and then call `pb::Serialize()` to perform the
serialization. For example:

```
#include <memory>
#include "audio_config.h"
#include "pb/serialize.h"

bool SendAudioConfigMessage(const AudioConfig& config) {
  auto wire_size = pb::ComputeSerializedSize(config);
  if (wire_size < 0) {
    return false;
  }
  std::unique_ptr<uint8_t[]> buffer(new uint8_t[wire_size]);
  pb::Serialize(config, buffer.get());  // No possible error; so, no return
                                        // value from this function.

  return SomeFunctionThatSendsTheBytes(buffer.get(), wire_size);
}
```

See `pb/examples_unittest.cc` for a number of usage examples.

## Default Values

The `AudioConfig` struct above doesn't initialize any of its members. This is
actually bad practice since, when parsing, fields that were not present in a
wire-format message will not be set. This will lead to undefined behavior in
your application!

**This library will not default-set to "zero" or "empty" any of your fields.**
Instead, parsing always executes *merge behavior*, which is discussed later.

To solve this problem, simply default-initialize in the C++ declaration, like
so:

```
struct AudioConfig {
  int32_t sample_rate = 48000;
  int32_t bit_depth = 16;
  int32_t channel_count = 2;
  pb::sint32_t delay_adjustment_ms = 0;
  pb::fixed64_t destination_id = 0;

  ...
};
```

The above may not always be what you want. Meaning, when there is missing data,
you may not want to imply a default value. Instead, you might want to handle
that as an error in your application. This brings us to the next section of this
document...

## Required versus Optional fields

Consider the following example:

```
// message AudioOfferringResponse {
//   enum Outcome { kSuccess = 0; kFail = 1; }
//   required Outcome result = 1;
//   optional string name = 2;
//   required int32 config_index = 3;
// }
struct AudioOfferringResponse {
  enum { kSuccess = 0, kFail = 1 } result;
  std::optional<std::string> name;
  int32_t config_index;

  using ProtobufFields = pb::FieldList<
      pb::Field<&AudioOfferringResponse::result, 1>,
      pb::Field<&AudioOfferringResponse::name, 2>,
      pb::Field<&AudioOfferringResponse::config_index, 3>>;
};
```

Required fields are almost always a bad idea, and Google's own Protocol Buffers
documentation says so. The reason is because they make it difficult to remove
useless fields in later versions of code.

This library takes the approach of treating all fields as optional when parsing.
That means nothing will check that a "required" field was populated during a
successful parse. If such a check is necessary, there should be some way of
detecting that a "required" field was missing in your code. For example, set the
data member to a *sentinel value* beforehand and check for it after the parse.
For convenience, C++ data members can use `std::optional<>` or
`std::unique_ptr<>` as an aid (i.e., the *sentinel value* would be
`std::nullopt` or `nullptr`).

When serializing, all fields will be emitted, except those whose data members
use a `std::optional<>` set to `std::nullopt`, a `std::unique_ptr<>` that is
null, or an iterable container that is empty.

## Repeated Fields

Use virtually any STL sequence or associative container, or any invention of
your own that integrates with the STL, and it will automatically be treated as a
protobuf `repeated` field. Example:

```
// message AudioOfferring {
//   repeated AudioConfig configs = 1;
// }
struct AudioOfferring {
  std::vector<AudioConfig> configs;

  using ProtobufFields = pb::FieldList<
      pb::Field<&AudioOfferring::configs, 1>>;
};
```

In the above, you could have used `std::list`, `std::deque`, etc. If using your
own invention, it should provide STL-container-like members and constructs,
like:

- For serialization: Iterators, and `std::begin()` and `std::end()` support.

- For parsing: `iterator insert(const_iterator pos, value_type&&... args);`
  where the `pos` argument will always be `std::end(container)`.

### Maps

Proto3's Maps feature is also supported, for convenience's sake. Example:

```
// message Registration {
//   map<string, int32> student_ages = 1;
// }
struct Registration {
  std::map<std::string, int32_t> student_ages;  // Name→Age

  using ProtobufFields = pb::FieldList<
      pb::Field<&Registration::student_ages, 1>>;
};
```

In addition, other STL-map-like containers will work, such as
`std::unordered_map` or your own inventions, where iteration and insert
operations use `std::pair` for the key+value pairs.

## Merge Behavior

When parsing, each successive parse into the same struct/class instance is
**additive**. That means existing data in containers (i.e., repeated fields)
will be preserved, with the data elements from the second parse being appended.
Non-repeated fields will be overwritten if they are encountered during the
second parse, but will not be modified if they are not encountered.

See notes below, on `std::string_view` for possible dangers.

## Enums

Both `enum {...}` and `enum class {...}` are supported. In addition, enums may
be declared with a specific underlying type (e.g., `enum : uint8_t {...}`). The
only limitation here is that imposed by the Protocol Buffers design: The
underlying type must be an integer type of 32 bits or fewer (bool is also
valid). This is checked for you at compile time.

Also note that, during either serialization or parsing, enum values are not
checked for validity. If it's possible for a data stream to transmitted over an
untrusted and/or unreliable channel, or is otherwise subject to possible data
corruption, you should check that a parsed enum contains a valid value.
Otherwise, your program could exhibit **undefined behavior**!

## Strings and String Views

Both `std::string` and `std::string_view` are treated as protobuf
`string`/`bytes`. There is no check to ensure they contain valid UTF-8
code-points. They are just raw byte strings.

`std::string_view` support is provided as an optimization, to reduce the copying
of large strings in your application. It is a *loaded gun*, in that you will
need to ensure proper memory management to prevent *shooting yourself in the
foot*.

Before serializing, a `std::string_view` data member should either be assigned a
valid memory pointer data+size, or should be set to *null* (the
default-constructed string_view). When the serializer comes across a null
`std::string_view` field, it simply interprets it as on optional field that is
not set.

When the parser encounters a `std::string_view` field, the associated data
member is assigned a memory region pointing within the wire-format buffer being
parsed. Thus, you must take care to ensure the buffer memory remains valid until
after any `std::string_view`s would be used.

When the parser does not encounter a `std::string_view` field in the wire bytes,
it leaves the data member unchanged. Thus, for safety, all `std::string_view`
members should be set to *null* (the default-constructed string_view) before a
parse, to ensure none are pointing to invalid memory.

## Nested Messages

Messages can be parsed/serialized within other messages. In the C++ code,
structs/classes can contain nested structs/classes or, just like `.proto` files,
C++ structs/classes can be used from different namespaces, imported from outside
code modules, etc.

In addition, messages are allowed to contain members that are self-referential.
In this case, std::unique_ptr<T> can be used in C++ code. Example:

```
// message BinaryTree {
//   message Node {
//     string value = 1;
//     optional Node left_child = 2;
//     optional Node right_child = 3;
//   }
//   optional Node root = 1;
// }
struct BinaryTree {
  struct Node {
    std::string value;
    std::unique_ptr<Node> left_child;
    std::unique_ptr<Node> right_child;

    bool is_leaf() const {
      return !left_child && !right_child;
    }

    using ProtobufFields = pb::FieldList<
        pb::Field<&Node::value, 1>,
        pb::Field<&Node::left_child, 2>,
        pb::Field<&Node::right_child, 3>>;
  };

  std::unique_ptr<Node> root;

  using ProtobufFields = pb::FieldList<pb::Field<&BinaryTree::root, 1>>;
};
```

`std::unique_ptr<>` can be used in repeated fields too. Example:

```
// message BTree {
//   message Node {
//     repeated sint32 keys = 1;
//     repeated Node children = 2;
//     repeated std::string data_strings = 3;
//   }
//   Node root;
// }
struct BTree {
  struct Node {
    std::vector<pb::sint32_t> keys;
    std::vector<std::unique_ptr<Node>> children;
    std::vector<std::string> data_strings;

    bool is_leaf() const {
      return children.empty() && !data_strings.empty();
    }

    using ProtobufFields = pb::FieldList<
        pb::Field<&Node::keys, 1>,
        pb::Field<&Node::children, 2>,
        pb::Field<&Node::data_strings, 3>>;
  };

  Node root;

  using ProtobufFields = pb::FieldList<pb::Field<&BTree::root, 1>>;
};
```

## Large Objects

If there are concerns about performance when moving/copying nested messages
around, then `std::unique_ptr<>` can be used to have the parser heap-allocate
any field that is parsed. This way, the object can be safely passed throughout
the application without having to make a copy.

# Inspection and Debugging

`inspection.h` includes a public API for analyzing a buffer and providing a
human-readable interpretation (as multi-line UTF-8 text) of the protobuf
structure and data within. As there is no exact way to deduce the message
structure from the wire bytes in all circumstances, sometimes multiple
interpretations will be provided and/or heuristics may provide a best-guess at
the data.

`protobuf_dump` is a simple program to inspect binary dump files, by invoking
the functions found in `inspection.h` and printing the results to
standard-out. It can be invoked as `protobuf_dump input.bin`, or with no
command-line arguments to read from standard-in. The input need not be
well-formed (e.g., if it is a dump of something that contains protobuf and
non-protobuf parts intermixed), but *beware that such input could throw-off the
interpretation in wild, unpredictable ways*.

For example, an input of
`0A....!fixed_64....B,The quick brown fox jumps over the lazy dog.....`
produces:

```
00000000  30 41                                            [6] = (u)intXX{65} | sintXX{-33}
00000000        2e 2e 2e 2e                                ....
00000000                    21 66 69 78 65 64 5f 36 34     [4] = double{3.56416e-57} | (s)fixed64{3762299423518386534}
00000000                                               2e  .
00000010  2e 2e 2e                                         ...
00000010           42 2c 54 68 65 20 71 75 69 63 6b 20 62  [8] = 44-char UTF-8: The quick b
00000020  72 6f 77 6e 20 66 6f 78 20 6a 75 6d 70 73 20 6f      rown fox jumps o
00000030  76 65 72 20 74 68 65 20 6c 61 7a 79 20 64 6f 67      ver the lazy dog
00000040  2e                                                   .
00000040     2e 2e 2e 2e                                   ....
```

The left-hand column shows the input offset (hex). The middle shows the byte
values (hex) of the input being interpreted. The right-hand column shows the
best-effort interpretation(s).

The first two bytes `0` and `A` are interpreted, respectively, as field number
`6` in a message and it's one-byte integer value. The value could be interpreted
as either: 1) the value `65` (decimal; ASCII for a captial A) for the `int32`,
`int64`, `uint32` or `uint64` data types; or 2) the value `-33` for the `sint32`
or `sint64` data types (a zig-zag encoded integer).

The following 4 dots are cannot be interpreted as anything, so they are dumped
verbatim (i.e., they are surrounding "garbage" as far as the parser is
concerned).

Then, the `!` is interpreted as field number `4` with a 8-byte value
(`fixed_64`) following it. We can see that `fixed_64` is an ASCII encoding of
the bytes `66` `69` `78` `65` `64` `5f` `36` `34` (hex), and these bytes are
interpreted as a raw little-endian dump of a 64-bit double or a signed/unsigned
integer type.

And, again, another 4 dots cannot be interpreted as anything.

Finally, `B,` is interpreted as field number `8` with a length-delimited object
after it, one of: 1) a UTF-8 string, 2) a byte array, 3) packed repeated scalar
values; or 4) a nested message. The ',' specifies 44 bytes will follow. Here,
the inspection implementation uses heuristics to determine that it's not a
nested message, and that the byte values happen to be a valid UTF-8 encoding of
a character sequence; and so that is the interpretation result.

Finally, there are 4 more dots at the end of the input that are not interpreted
as anything.

# Build Details

## Integration with your application

As the implementation is entirely C++ templates, you need only `#include` the
necessary top-level headers where the functionality is used. Use
`pb/field_list.h` (and `pb/integer_wrapper.h`, if necessary) in modules where
your message struct/class declarations are made. Then, `#include` either
`pb/serialize.h` or `pb/parse.h` where serialization/parsing to/from buffers
happens. Thus, it should be trivial to include this library in your
application's build setup, regardless of choice (Makefile, CMake, GN, etc.).

All library code is currently intended to be used by C++17 (or later) language
compilers (as of this writing, tested with Clang 13.0.0 and GCC 11.1.0). C++11
or 14 may work with suitable modifications, but such support is not a goal of
this project. Upgrade your toolchain, since older compilers are always
guaranteed to have **known** bugs!

In addition, note that the library code does not throw exceptions, nor does it
require any C++ run-time type support. Thus, your application can be built with
those features turned off without any loss of functionality.

### Integration of Inspection/Debugging parts

For the `pb/inspection.h` support, or to build the protobuf_dump utility, you
will need to set up compilation/linking of the `.cc` modules associated with
them. See `BUILD.gn` for an easy-to-grok GN example, which should be easily
translatable to whatever build system you are using. Or, continue reading with
the next section...

## Setting up a development environment

Assuming you have already cloned this repository to your development machine,
start by having git fetch the submodules:

```
;; cd <protobuf_super_lite basedir>
git submodule init
git submodule update
```

It goes without saying that you will also need a compiler toolchain set up. This
project is meant to be buildable using either LLVM/Clang (default) or GNU
G++. LLVM/Clang is preferred, as it also comes with many useful
correctness-checking tools, such as AddressSanitizer and fuzz testing.

This project uses Chromium GN to generate Ninja build files. GN+Ninja was chosen
for it simplicity and build file readability (even for first-timers), and
because it takes very litte effort to set up. Most operating systems have a
pre-built Ninja available in their package repository. For Debian-like systems,
the following command will install it:

```
sudo apt-get install ninja-build
```

Prebuilt GN binaries can also be downloaded, but there are far fewer availble
(i.e., for all the various platforms out there). Thus, this project includes GN
as a git submodule in the `third_party/gn` directory. To build it:

```
;; NOTE: GN dependencies must be installed first, e.g.:
;;
;;   sudo apt-get install python libatomic1

cd third_party/gn

python build/gen.py

ninja -C out
```

If all went well, there should be a working GN binary in the `out` directory.
Confirm this with the following command:

```
out/gn --version
```

Finally, with all of the above ready-to-go, it's time to build this project.

The build output files and final libraries/binaries will be written to the
`out/Default` directory:

```
;; cd <protobuf_super_lite basedir>
mkdir -p out/Default
```

Next, configure the build for debug mode, and turn on Clang's Address Sanitizer:

```
(echo 'is_debug = true'; echo 'is_asan = true') > out/Default/args.gn
```

*Alternatively*, if using GNU G++, you must configure the build system for G++
(and note that ASAN will not be available). Thus, `args.gn` should instead be
generated as:

```
(echo 'is_debug = true'; echo 'use_gcc_instead_of_llvm = true') \
    > out/Default/args.gn
```

Then, invoke GN to generate the ninja build files:

```
third_party/gn/out/gn gen out/Default
```

Finally, run ninja to build the project:

```
ninja -C out/Default
```

And, you should be able to run the unit test suite successfully now:

```
out/Default/protobuf_unittests
```

As you make changes to the code, you only need to re-run ninja to incrementally
build. That's it!

# Other

## Examples

The code samples provided in this document can be found in
`pb/examples_unittest.cc`, along with serializing and parsing code, and simple
demo logic.

## Missing Topics

Generally, if a topic has not been explicitly discussed in this document, the
intention is to follow the design described in Google's public [Protocol Buffers
Documentation](https://developers.google.com/protocol-buffers/docs/encoding) as
closely as possible. proto3 default behaviors take precedence over proto2 (e.g.,
encoding repeated scalar fields using the *packed encoding* scheme).

Additionally, the document linked above provides useful do's and do-not's when
using protobufs in your application, between applications running on separate
machines (e.g., server-client), between applications using different languages
or on different platforms, and when considering the backwards-compatibility of
potential proto message changes as your application evolves.

## Coding Style

This project follows the [Chromium C++ Style Guide](
https://chromium.googlesource.com/chromium/src/+/HEAD/styleguide/c++/c++.md) for
coding style, which is an extension of the [Google C++ Style Guide](
https://google.github.io/styleguide/cppguide.html). However, note the following
exceptions:

- Use of `std::size_t` should be limited to code interfacing with STL APIs. For
  pointer offsets or pointer arithmetic, prefer `std::ptrdiff_t`, which is a
  signed type. For allocation sizes, generally `int32_t` is preferred; and then,
  in rare cases, use `int64_t` when adding/multiplying `int32_t` together, if
  overflow is possible.

- Certain mentions, in the style guide, of Chromium-project-specific
  infrastructure are obviously moot (e.g., logging,
  `base/numerics/safe_conversions.h`, and UTF-16 use).

## Features Not Implemented

### Not-yet implemented

- The proto3 Oneof feature. This is very much like a std::variant.

### Likely never to be implemented

- Reserved Fields
- Extensions
- Wire types 3 or 4: Groups. They are deprecated anyways, and not widely used.
- The proto3 "Any" message type.

### C++20

Much of this library's internal template code could be made more-readable by
using C++20 features. However, to ensure maximum availability across platforms
and toolchains (as of this writing), C++17 is the minimum requirement.

It's possible that the library will be forked at some point into separate
C++17-compatible and C++20 branches, and "not-yet implemented" features only
implemented in the C++20 branch going forward.

It's always a good thing to pressure the compiler vendors and the hardware
vendors to move C++ into the future, rather than developers remaining stuck in
its *clunky* past...
