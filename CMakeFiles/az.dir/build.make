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

# Utility rule file for az.

# Include any custom commands dependencies for this target.
include CMakeFiles/az.dir/compiler_depend.make

# Include the progress variables for this target.
include CMakeFiles/az.dir/progress.make

CMakeFiles/az: az-gxde-terminal.gmo

az-gxde-terminal.gmo: po/az/LC_MESSAGES/gxde-terminal.po
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --blue --bold --progress-dir=/home/gfdgd_xi/Desktop/dde15/gxde-terminal-gtk/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Generating az-gxde-terminal.gmo"
	/usr/bin/msgfmt -o /home/gfdgd_xi/Desktop/dde15/gxde-terminal-gtk/az-gxde-terminal.gmo po/az/LC_MESSAGES/gxde-terminal.po

az: CMakeFiles/az
az: az-gxde-terminal.gmo
az: CMakeFiles/az.dir/build.make
.PHONY : az

# Rule to build all files generated by this target.
CMakeFiles/az.dir/build: az
.PHONY : CMakeFiles/az.dir/build

CMakeFiles/az.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/az.dir/cmake_clean.cmake
.PHONY : CMakeFiles/az.dir/clean

CMakeFiles/az.dir/depend:
	cd /home/gfdgd_xi/Desktop/dde15/gxde-terminal-gtk && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/gfdgd_xi/Desktop/dde15/gxde-terminal-gtk /home/gfdgd_xi/Desktop/dde15/gxde-terminal-gtk /home/gfdgd_xi/Desktop/dde15/gxde-terminal-gtk /home/gfdgd_xi/Desktop/dde15/gxde-terminal-gtk /home/gfdgd_xi/Desktop/dde15/gxde-terminal-gtk/CMakeFiles/az.dir/DependInfo.cmake "--color=$(COLOR)"
.PHONY : CMakeFiles/az.dir/depend

