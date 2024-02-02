#!/bin/bash

starttag="<!-- LIBRARY_SIZE_START -->"
endtag="<!-- LIBRARY_SIZE_END -->"

readme=`cat README.md`

canprint=true

clangversion=`clang --version | head -n 1 | cut -d" " -f3`
echo "Using clang version $clangversion." 1>&2

compilers=(
	"clang -c modplay.c -o modplay.o -target amd64 -Os"
	"clang -c modplay.c -o modplay.o -target i386 -Os"
	"clang -c modplay.c -o modplay.o -target aarch64 -Os"
	"clang -c modplay.c -o modplay.o -target armv7m -Os"
#	"clang -c modplay.c -o modplay.o -target riscv64"
#	"clang -c modplay.c -o modplay.o -target riscv32"
)

compilersfriendly=(
	"clang $clangversion"
	"clang $clangversion"
	"clang $clangversion"
	"clang $clangversion"
#	"clang $clangversion"
#	"clang $clangversion"
)

archsfriendly=(
	"amd64"
	"i386"
	"aarch64"
	"armv7m"
)

optimizations=(
	"-O0"
	"-O3"
	"-Os"
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
				for k in "${!optimizations[@]}"; do
					rm -f modplay.o
					compiler="${compilers[$i]} ${channels[$j]} ${optimizations[$k]}"
					echo $compiler 1>&2
					$compiler
					size=`size modplay.o | grep modplay.o`
					textsize=`echo "$size" | awk '{print $1}'`
					datasize=`echo "$size" | awk '{print $2}'`
					bsssize=`echo "$size" | awk '{print $3}'`
					rm -f modplay.o
					echo "|${compilersfriendly[$i]} (${optimizations[$k]})|${archsfriendly[$i]}|$textsize|$datasize|$bsssize|"
				done
			done

			echo
		done

		echo "Done generating." 1>&2
	fi
done > README.md

