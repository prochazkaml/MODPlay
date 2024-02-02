#!/bin/bash

starttag="<!-- LIBRARY_SIZE_START -->"
endtag="<!-- LIBRARY_SIZE_END -->"

readme=`cat README.md`

canprint=true

clangversion=`clang --version | head -n 1 | cut -d" " -f3`
echo "Using clang version $clangversion." 1>&2

compilers=(
	"clang -c modplay.c -o modplay.o -target amd64"
	"clang -c modplay.c -o modplay.o -target i386"
	"clang -c modplay.c -o modplay.o -target aarch64"
	"clang -c modplay.c -o modplay.o -target armv7m"
#	"clang -c modplay.c -o modplay.o -target riscv64"
#	"clang -c modplay.c -o modplay.o -target riscv32"
)

compilersfriendly=(
	"clang $clangversion|amd64"
	"clang $clangversion|i386"
	"clang $clangversion|aarch64"
	"clang $clangversion|armv7m"
#	"clang|riscv64"
#	"clang|riscv32"
)

channels=(
	""
	"-DCHANNELS=4"
)

channelsfriendly=(
	"with the default number of channels"
	"for 4 channels maximum"
)

echo "$readme" | while read line; do
	if [ "$line" == "$endtag" ]; then
		canprint=true
	fi

	if $canprint; then
		echo $line
	fi

	if [ "$line" == "$starttag" ]; then
		canprint=false

		for j in "${!channels[@]}"; do
			echo "Compiled ${channelsfriendly[$j]}:"
			echo
			echo "|Compiler|Arch|.text|.data|.bss"
			echo "|-|-|-|-|-|"
			
			echo "Generating compiler table..." 1>&2

			for i in "${!compilers[@]}"; do
				rm -f modplay.o
				compiler=${compilers[$i]}
				compilerfriendly=${compilersfriendly[$i]}
				echo "Running \"$compiler\"" 1>&2
				$compiler ${channels[$j]}
				size=`size modplay.o | grep modplay.o`
				textsize=`echo "$size" | awk '{print $1}'`
				datasize=`echo "$size" | awk '{print $2}'`
				bsssize=`echo "$size" | awk '{print $3}'`
				rm -f modplay.o
				echo "|$compilerfriendly|$textsize|$datasize|$bsssize|"
			done

			echo
		done

		echo "Done generating." 1>&2
	fi
done > README.md

