<a href="https://promisesaplus.com/">
    <img src="https://promisesaplus.com/assets/logo-small.png" alt="Promises/A+ logo"
         title="Promises/A+ 1.1 compliant" align="right" />
</a>

# upromise

A javescript-like promise library for C/C++.

## features

- Promises/A+ 1.1 compliant (except for arbitrary types as arguments)
- Completely implemented in C
- Provide C++ binding in the same header file and provide `Thenable`
- The C language part only uses the standard library and ucontext (using the functional encapsulation provided by the [corountine](https://github.com/cloudwu/coroutine) library)
- Complete porting of [Promises/A+ tests](https://github.com/promises-aplus/promises-tests) to C++

## roadmap

- [ ] generator function related helper functions
- [ ] async/await related helper functions