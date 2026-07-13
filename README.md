# UNIMORE COMPILER COURSE 2025/2026

Made by Stefano casolari (mat 187572)

## Assignment 1

### Algebraic identity
Simplify algebriac identity (x+0, x*1, 0/x...)

### strenght reduction
Simplifies Mul instruction to a series of shift instruction. the algoritm works with a max number of shifts additions (by default it's 64 which effectively turns off this feature, it would be better to set it to a low number for real world applications) in order to avoid having the series of additions and subrtaction be more costly in cpu cycle than a standard mul instruction

divisions (both signed and unsigned) will only be simplified if the divisor is a multiple of 2

### multi instruction optimization

Simplifies additions and subraction of the form:
$$<x>op<k>$$
with k constant. The pass will look at x's users and look for instruction with a similar structure. the constant will be added/subrtacted and the original instruction will be updated. (More information in the source file)
## Assignment 2
Data flow analysis, check pdf

## Assignment 3

Loop invariant code motion (LICM), looks for instruction in loops that are invariant and then evaluates them for code motion. if all test pass then the instruction will be moved to the preheader

## Assignment 4

Loop fusion: fuse 2 loops if the condition are met (see source file for more information). do note that this pass requires loop to be in simple form (in other words they have one backedge only, from the latch to the header) along some other prerequisites (see source file). in order to achieve so (for testing purposes) the source file is first optimized with the simplify loop pass (see run.sh) before being fed to the loop fusion pass.