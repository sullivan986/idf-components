## Using Tutorials
Add  variables `EXTRA_COMPONENT_DIRS` to `CMakeLists.txt` in the project, for example:

`
list(APPEND EXTRA_COMPONENT_DIRS "/path/to/this/repo")
`

or add this repo to the `components` folder in the root of project.

there are some notice:

- Be sure to add `CONFIG_COMPILER_CXX_EXCEPTIONS=y` to the `sdkconfig.defaults` file when using ASIO.