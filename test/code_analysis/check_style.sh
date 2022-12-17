#!/usr/bin/env bash

check_for_functions_to_avoid() {

	echo "Checking for functions to avoid..."

	# There are some C functions that are tricky to use and it's preferred to
	# avoid using them. If any function in this list is really needed
	# we need to review the usage and add an exception here.
	local functions="free malloc calloc strcpy wcscpy strcat wcscat sprintf vsprintf strtok ato strlen strcmp wcslen alloca vscanf vsscanf vfscanf scanf sscanf fscanf strncat strsep toa memmove asctime getwd gets basename dirname system setjmp longjmp"

	local exceptions
	declare -A exceptions

	local exception_label="ignore_function"

	# If multiple exceptions for the same function they should be separated by a # character
	exceptions["free"]="free(ptr);"

	for func in $functions; do
		local base_grep_cmd="grep \"\<$func(\" $PROJECT_DIR/src $PROJECT_DIR/bin --include \"*.c\" --include \"*.h\" -R | grep -v $exception_label"
		if [ -n "${exceptions[$func]}" ]; then
			IFS=$'\n'
			filters=$(echo "${exceptions[$func]}" | tr "#" "\n")
			for i in $filters; do
				base_grep_cmd="$base_grep_cmd | grep -v \"$i\""
			done
			unset IFS
		fi
		if eval "$base_grep_cmd" > /dev/null 2>&1; then
			echo "Found \"$func\" on files:"
			eval "$base_grep_cmd"
			echo ""
			failure=1
		fi
	done

}

check_for_missing_linebreak_on_log() {

	echo "Checking for missing line breaks on log messages..."

	# Looking for missing \n on log messages calls.
	# One limitation of this check is that it only looks for calls that are one
	# liners.
	local functions="DEBUG_PRINT"

	local exception_label="ignore_linebreak"

	for func in $functions; do
		local base_grep_cmd="grep \"\<$func(\" $PROJECT_DIR/src $PROJECT_DIR/bin --include \"*.c\" --include \"*.h\" -R | grep \")\" | grep -v \"\\\\\\n\" | grep -v $exception_label"
		if eval "$base_grep_cmd" > /dev/null 2>&1; then
			echo "line break not found on call to \"$func\" on files:"
			eval "$base_grep_cmd"
			echo ""
			failure=1
		fi
	done

}

check_for_missing_param_direction() {

	echo "Checking for missing parameter direction in functions..."

	# we use the following standard when writing function documentation
	# /**
	# * @brief Some description.
	# *
	# * @param[in] param_in dummy input param
	# * @param[out] param_out dummy output param
	# */
	# This check catches missing [direction] in @param
	base_grep_cmd="grep -R \"@param \" $PROJECT_DIR/src $PROJECT_DIR/bin $PROJECT_DIR/test --include \"*.c\""
	if eval "$base_grep_cmd" > /dev/null 2>&1; then
		echo "Found @param without [in]/[out] on files:"
		eval "$base_grep_cmd"
		echo ""
		failure=1
	fi
}

check_for_trailing_whitespaces() {

	echo "Checking for trailing white space..."

	# Make sure for whitespace at the end of lines of code
	cmd="grep -r '[[:blank:]]$' $PROJECT_DIR/test $PROJECT_DIR/src $PROJECT_DIR/bin -l --include \*.sh --include \*.c --include \*.h"
	if eval "$cmd" > /dev/null 2>&1; then
		echo "Trailing whitespaces in files:"
		eval "$cmd"
		echo ""
		failure=1
	fi
}

check_for_brackets_in_if_statements() {

	echo "Checking for missing brackets in single line if statements..."

	cmd="git grep if\ \(.*\)$ -- ${C_SOURCES[*]}"
	if eval "$cmd" > /dev/null 2>&1; then
		echo "Missing brackets in single line if statements:"
		eval "$cmd"
		echo ""
		failure=1
	fi

	cmd="git grep else$ -- ${C_SOURCES[*]}"
	if eval "$cmd" > /dev/null 2>&1; then
		echo "Missing brackets in single line else statements:"
		eval "$cmd"
		echo ""
		failure=1
	fi
}

check_shell_scripts_code_style() {

	echo "Checking for style compliance in shell scripts..."

	# Make sure shell scripts follow coding good practices
	find "$PROJECT_DIR" -type f \( -name '*.sh' -o -name '*.bash' -o -name '*.ksh' \) -print0 | xargs -0 shellcheck || failure=1
}

check_clang_style() {

	echo "Checking for style compliance in C source code..."

	cmd_diff="git diff --exit-code $PROJECT_DIR/src $PROJECT_DIR/test $PROJECT_DIR/examples $PROJECT_DIR/bin"
	if ! eval "$cmd_diff" > /dev/null 2>&1; then
		echo "Error: can only check code style when src/ and test/ is clean."
		echo "Stash or commit your changes and try again."
		echo ""
		failure=1
		return
	fi

	cmd="find $PROJECT_DIR -type f -name '*.[ch]' -not -path '$PROJECT_DIR/build/*' -exec clang-format -i -style=file {} +"
	if ! eval "$cmd" > /dev/null 2>&1; then
		echo "Error: could not run clang-format. Make sure you have clang-format installed."
		echo ""
		failure=1
		return
	fi

	if ! eval "$cmd_diff" > /dev/null 2>&1; then
		echo "Code style issues found. Run 'git diff' to view issues."
		echo ""
		failure=1
		return
	fi
}

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
PROJECT_DIR=$(dirname "$(dirname "$SCRIPT_DIR")")
C_SOURCES=(
	"$PROJECT_DIR"/src/*.c
	"$PROJECT_DIR"/test/*.c
	"$PROJECT_DIR"/test/system/*.c
	"$PROJECT_DIR"/test/system/helpers/*.c
	"$PROJECT_DIR"/test/unit/*.c
	"$PROJECT_DIR"/test/unit/helpers/*.c
	"$PROJECT_DIR"/examples/*.c
)


# Make sure the code follows the coding style defined by the project
failure=0
check_for_functions_to_avoid
check_for_missing_linebreak_on_log
check_for_missing_param_direction
check_for_trailing_whitespaces
check_shell_scripts_code_style
check_clang_style
check_for_brackets_in_if_statements

exit "$failure"
