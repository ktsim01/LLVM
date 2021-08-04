# LLVM Compiler
## Dependencies
- Flex
- Bison
- LLVM 11.0.0
- 'llvm-config' should be in bin/path

## Instructions To Run
Use the Dockerfile contained in .devcontainer to setup an environment with the correct dependencies. The VS Code Docker extension will automatically prompt you to do this once you open up the project.

Alternatively, you can also use Docker manually.
```
cd .devcontainer
docker build -t <name> .
cd ..
docker run -v $(pwd):/project -it <name>
cd project
```
From here, you can just use build the project normally. Run `./out -h` for more information.
```
make 
./out <source_file> -o <object_file>
gcc -static <object_file>
./a.out
```
A small example program is included in `abc.txt`.