# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.29

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:

#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:

# Disable VCS-based implicit rules.
% : %,v

# Disable VCS-based implicit rules.
% : RCS/%

# Disable VCS-based implicit rules.
% : RCS/%,v

# Disable VCS-based implicit rules.
% : SCCS/s.%

# Disable VCS-based implicit rules.
% : s.%

.SUFFIXES: .hpux_make_needs_suffix_list

# Command-line flag to silence nested $(MAKE).
$(VERBOSE)MAKESILENT = -s

#Suppress display of executed commands.
$(VERBOSE).SILENT:

# A target that is always out of date.
cmake_force:
.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/bin/cmake

# The command to remove a file.
RM = /usr/bin/cmake -E rm -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/gfdgd_xi/Desktop/dde15/gxde-terminal-gtk

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/gfdgd_xi/Desktop/dde15/gxde-terminal-gtk

# Utility rule file for ko.

# Include any custom commands dependencies for this target.
include CMakeFiles/ko.dir/compiler_depend.make

# Include the progress variables for this target.
include CMakeFiles/ko.dir/progress.make

CMakeFiles/ko: ko-gxde-terminal.gmo

ko-gxde-terminal.gmo: po/ko/LC_MESSAGES/gxde-terminal.po
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --blue --bold --progress-dir=/home/gfdgd_xi/Desktop/dde15/gxde-terminal-gtk/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Generating ko-gxde-terminal.gmo"
	/usr/bin/msgfmt -o /home/gfdgd_xi/Desktop/dde15/gxde-terminal-gtk/ko-gxde-terminal.gmo po/ko/LC_MESSAGES/gxde-terminal.po

ko: CMakeFiles/ko
ko: ko-gxde-terminal.gmo
ko: CMakeFiles/ko.dir/build.make
.PHONY : ko

# Rule to build all files generated by this target.
CMakeFiles/ko.dir/build: ko
.PHONY : CMakeFiles/ko.dir/build

CMakeFiles/ko.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/ko.dir/cmake_clean.cmake
.PHONY : CMakeFiles/ko.dir/clean

CMakeFiles/ko.dir/depend:
	cd /home/gfdgd_xi/Desktop/dde15/gxde-terminal-gtk && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/gfdgd_xi/Desktop/dde15/gxde-terminal-gtk /home/gfdgd_xi/Desktop/dde15/gxde-terminal-gtk /home/gfdgd_xi/Desktop/dde15/gxde-terminal-gtk /home/gfdgd_xi/Desktop/dde15/gxde-terminal-gtk /home/gfdgd_xi/Desktop/dde15/gxde-terminal-gtk/CMakeFiles/ko.dir/DependInfo.cmake "--color=$(COLOR)"
.PHONY : CMakeFiles/ko.dir/depend

