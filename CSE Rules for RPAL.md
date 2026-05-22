| Rule | CONTROL | STACK | ENV |
|---|---|---|---|
| Initial State | `e₀ δ₀` | `e₀` | `e₀ = PE` |
| CSE Rule 1 (stack a name) | `.... Name ....` | `.... Ob ....` | `Ob = Lookup(Name, e_c)`<br>`e_c = current environment` |
| CSE Rule 2 (stack λ) | `.... λ_x^k ....` | `.... c_x^k ....` | `e_c = current environment` |
| CSE Rule 3 (apply rator) | `.... γ ....` | `.... Rator Rand ....`<br>`.... Result ....` | `Result = Apply[Rator, Rand]` |
| CSE Rule 4 (apply λ) | `.... γ ....`<br>`.... c_x^k ....` | `.... Rand ....`<br>`.... e_n ....` | `e_n = [Rand/x]e_c` |
| CSE Rule 5 (exit env.) | `.... e_n ....` | `.... value e_n ....`<br>`.... value ....` | — |

# Optimizations for the CSE Machine

## CSE Rules 6 and 7: Unary and Binary Operators

| Rule | CONTROL | STACK | ENV |
|---|---|---|---|
| **CSE Rule 6 (binop)** | `.... binop` | `Rand Rand ....`<br>`Result ....` | `Result = Apply[binop, Rand, Rand]` |
| **CSE Rule 7 (unop)** | `.... unop` | `Rand ....`<br>`Result ....` | `Result = Apply[unop, Rand]` |

---

# CSE Rule 8: Conditional

| Rule | CONTROL | STACK | ENV |
|---|---|---|---|
| **CSE Rule 8 (Conditional)** | `.... δ_then δ_else β`<br>`.... δ_then` | `true ....` | `....` |
|  | `.... δ_then δ_else β`<br>`.... δ_else` | `false ....` | `....` |

---

# CSE Rules 9 and 10: Tuples

| Rule | CONTROL | STACK | ENV |
|---|---|---|---|
| **CSE Rule 9 (tuple formation)** | `.... τ_n`<br>`....` | `V₁ .... Vₙ ....`<br>`(V₁,.....,Vₙ) ....` | `....` |
| **CSE Rule 10 (tuple selection)** | `.... γ`<br>`....` | `(V₁,.....,Vₙ) I ....`<br>`V_I ....` | `....` |

### CSE Rule 11: n-ary functions

| CONTROL | STACK | ENV |
|----------|--------|-----|
| `... γ`<br>`... e_m δ_k` | `c λ_k^{v_1,\ldots,v_n} Rand ...`<br>`e_m ...` | `e_m = [Rand_1 / V_1] ... [Rand_n / V_n] e_c` |

**Rule 11 (n-ary function):**

If the control contains the environment marker `e_m` followed by `δ_k`, and the stack contains an n-ary lambda closure

\[
c \lambda_k^{v_1,\ldots,v_n}
\]

together with arguments

\[
Rand_1,\ldots,Rand_n,
\]

then create a new environment

\[
e_m = [Rand_1/V_1]\cdots[Rand_n/V_n]e_c
\]

binding each formal parameter \(V_i\) to the corresponding argument \(Rand_i\).

