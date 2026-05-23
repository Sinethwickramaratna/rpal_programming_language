# CSE Rules for RPAL

This file is a compact reference for the control, stack, and environment rules
used by the RPAL CSE machine.

## Rule Summary

| Rule | Control | Stack | Environment |
|---|---|---|---|
| Initial state | `e₀ δ₀` | `e₀` | `e₀ = PE` |
| Rule 1: name lookup | `.... Name ....` | `.... Ob ....` | `Ob = Lookup(Name, e_c)`<br>`e_c = current environment` |
| Rule 2: lambda closure | `.... λ_x^k ....` | `.... c_x^k ....` | `e_c = current environment` |
| Rule 3: apply rator | `.... γ ....` | `.... Rator Rand ....`<br>`.... Result ....` | `Result = Apply[Rator, Rand]` |
| Rule 4: apply lambda | `.... γ ....`<br>`.... c_x^k ....` | `.... Rand ....`<br>`.... e_n ....` | `e_n = [Rand/x]e_c` |
| Rule 5: exit environment | `.... e_n ....` | `.... value e_n ....`<br>`.... value ....` | — |

## Optimized Machine Rules

### Rule 6 and Rule 7: Unary and Binary Operators

| Rule | Control | Stack | Environment |
|---|---|---|---|
| Rule 6: binary operator | `.... binop` | `Rand Rand ....`<br>`Result ....` | `Result = Apply[binop, Rand, Rand]` |
| Rule 7: unary operator | `.... unop` | `Rand ....`<br>`Result ....` | `Result = Apply[unop, Rand]` |

### Rule 8: Conditional

| Rule | Control | Stack | Environment |
|---|---|---|---|
| Rule 8: conditional | `.... δ_then δ_else β`<br>`.... δ_then` | `true ....` | `....` |
| Rule 8: conditional | `.... δ_then δ_else β`<br>`.... δ_else` | `false ....` | `....` |

### Rule 9 and Rule 10: Tuples

| Rule | Control | Stack | Environment |
|---|---|---|---|
| Rule 9: tuple formation | `.... τ_n`<br>`....` | `V₁ .... Vₙ ....`<br>`(V₁, ..... , Vₙ) ....` | `....` |
| Rule 10: tuple selection | `.... γ`<br>`....` | `(V₁, ..... , Vₙ) I ....`<br>`V_I ....` | `....` |

### Rule 11: n-ary Functions

| Control | Stack | Environment |
|---|---|---|
| `... γ`<br>`... e_m δ_k` | `c λ_k^{v_1,\ldots,v_n} Rand ...`<br>`e_m ...` | `e_m = [Rand_1 / V_1] ... [Rand_n / V_n] e_c` |

When the control contains the environment marker `e_m` followed by `δ_k`, and
the stack contains an n-ary lambda closure together with its arguments, the
machine creates a new environment:

\[
e_m = [Rand_1/V_1]\cdots[Rand_n/V_n]e_c
\]

Each formal parameter \(V_i\) is bound to the matching argument \(Rand_i\).

## Notes

- `e_c` means the current environment.
- `δ₀` is the initial delta body.
- Rule 5 restores the caller environment after a function body finishes.
- Rule 11 is the n-ary function case used when a lambda takes tuple-style parameters.

