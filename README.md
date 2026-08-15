## Design Decisions
### Why use `std::string` and `std::vector<std::string>` in the `Command` struct?

I use `std::string` and `std::vector<std::string>` because they automatically manage memory, so I don't have to manually allocate or free memory. This makes the code safer and avoids issues such as memory leaks and dangling pointers.
When executing external commands (e.g., with `execvp`), I convert the strings to C-style strings using `data()` (or `c_str()`) and build a `char*[]` argument array. This conversion does **not** allocate new memory. Instead, `data()` simply returns a pointer to the character buffer already owned by the corresponding `std::string`.
As a result, memory management is still handled entirely by the `std::string` objects, while the C API receives the `char*` pointers it requires.
