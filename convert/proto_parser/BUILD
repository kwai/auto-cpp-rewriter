import os

cc_library(
name = "proto_parser",
srcs = ["*.cc"],
deps = [
    "//third_party/nlohmann_json/BUILD:nlohmann_json",
    "//third_party/glog/BUILD:glog",
    "//third_party/abseil/BUILD:abseil",
],
cppflags = [
    "-Wno-unused-variable",
    "-Wno-unused-parameter",
    "-Wno-unknown-pragmas",
    "-Wno-unused-local-typedefs",
],
link_all_symbols = True
)
