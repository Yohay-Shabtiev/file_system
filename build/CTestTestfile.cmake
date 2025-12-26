# CMake generated Testfile for 
# Source directory: /home/yohay/projects/file_system/file_system
# Build directory: /home/yohay/projects/file_system/file_system/build
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(fs_tests "/home/yohay/projects/file_system/file_system/build/fs_tests")
set_tests_properties(fs_tests PROPERTIES  _BACKTRACE_TRIPLES "/home/yohay/projects/file_system/file_system/CMakeLists.txt;44;add_test;/home/yohay/projects/file_system/file_system/CMakeLists.txt;0;")
subdirs("_deps/googletest-build")
