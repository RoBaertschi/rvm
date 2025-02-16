# RVM Errors

How should RVM handle errors?


## Callbacks

The VM calls a callback on error, these callbacks could be registered at any point and deregistered again. When deregistered, the one before will take its place again. Basically Exceptions.

## Errors as value

Problems:
How can the language efficiently detect these? Well now theres a error on the stack, but how should the language now about that. How can the user of the VM know efficiently, without checking, if the operation failed.

## Solution

The bytecode can register a vm error callback at any time, they can then also be deregistered at any time. When there is a callback missing for a specific error, the instruction execution fails, else it will jump to the callback. A callback is a `function` object.
