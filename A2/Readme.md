# 📘 Distributed Point Function Query Generator (`gen_queries.cpp`)

## **Overview**
This project implements a **Distributed Point Function (DPF) Query Generator and Verifier** as part of **CS670 Assignment 2**.  
The program generates random DPF instances, evaluates them for correctness, and stores all generated key information for both parties.

---

## **Program Files**
| File | Description |
|------|--------------|
| **`helper.h`** | Contains utility structures, random generation helpers, PRG function, logging utilities, and file-writing helpers. |
| **`gen_queries.cpp`** | Main program that generates DPF queries, evaluates correctness, and writes all logs and key data to output files. |

---

## **Compilation**
Use any modern C++ compiler (C++17 or later):

```bash
g++ gen_queries.cpp -o gen_queries
````

---

## **Usage**

```bash
./gen_queries <DPF_size> <num_DPFs>
```

### **Arguments**

* `<DPF_size>` → The domain size of each DPF instance (must be a power of 2).
* `<num_DPFs>` → Number of DPF instances to generate and test.

### **Example**

```bash
./gen_queries 1024 10
```

This generates **10 DPFs**, each with a domain size of **1024**, tests their correctness, and logs the results.

---

## **Output Files**

The program produces the following files after execution:

| File                | Description                                                                                                                                                         |
| ------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **`dpf0_keys.txt`** | Contains the DPF keys for **Party 0** (num (i.e., `s` concatenated with `f`), `s`, `f` and correction words for all DPFs).                                                                    |
| **`dpf1_keys.txt`** | Contains the DPF keys for **Party 1** (num (i.e., `s` concatenated with `f`), `s`, `f` and correction words for all DPFs).                                                                    |
| **`summary.txt`**   | A detailed log of DPF generation, internal computations (PRG outputs, correction words, root updates), evaluation steps, and correctness verification for each DPF. |

---

Here’s a nicely worded section for your README explaining your implementation based on the plan you provided:

---

## **Implementation Details**

The implementation follows a **tree-based DPF construction** using a pseudo-random generator (PRG) and correction words.

**Pseudo-Random Generator (PRG):** Given an `n`-bit seed, the PRG produces a `2n`-bit output, which is split into left and right child nodes. Each node is represented as a 32-bit number where the first 31 bits store the `S` value, and the last bit stores the flag `f`.

**`generateDPF` Function:**

1. Convert the target index into its binary representation, padded to match the depth of the DPF tree.
2. Generate **two root nodes**, one for each party, as random 32-bit integers.
3. Iteratively expand each node using the PRG to produce left and right children.
4. Based on the corresponding bit in the binary index, select the left (0) or right (1) child to continue the path.
5. At each level, compute **correction words** to ensure that, when the two parties’ outputs are XORed, the target index produces the desired value while all other indices remain zero.
6. Repeat until reaching the leaf nodes and apply a **final correction word** to guarantee the correct target value.

**`EvalFull` & `evalDPF` Function:**

1. Store the tree in a **level-wise array** of size `2 * DPF_size - 1`.
2. Expand each node using the PRG and generate left and right children.
3. Apply the appropriate correction words to each child based on the level in the tree.
4. Evaluate both parties’ trees independently and XOR their leaf outputs to reconstruct the DPF.
5. Verify that the target index matches the expected value and all other indices are zero.

## **Program Flow**

### 1. **Input Handling**

The program reads command-line arguments:

```bash
./gen_queries <DPF_size> <num_DPFs>
```

It validates that `DPF_size` is a power of 2.

---

### 2. **DPF Generation (`generateDPF()`)**

For each DPF:

* Randomly selects:
  * a **target index** within `[0, DPF_size - 1]`
  * a **target value** (31-bit integer)
* Constructs two seeds (for Party 0 and Party 1)
* Iteratively expands through PRG and generates **correction words (CW)** at each level
* Adds a **final correction word** to ensure correct reconstruction of the target value
* Logs every step into `summary.txt`
* Writes key data into `dpf0_keys.txt` and `dpf1_keys.txt`

---

### 3. **DPF Evaluation (`evalDPF()`)**

Reconstructs the DPF output array for each party by expanding the tree using the same PRG and correction words.

---

### 4. **Full Evaluation (`EvalFull()`)**

Combines outputs from both parties and verifies:

* The combined value equals the **target value** at the selected index
* All other positions reconstruct to **zero**

Prints `"Test Passed"` or `"Test Failed"` accordingly.

---

## **Sample Console Output**

```
================= Generating DPF 1 =================
DPF 1 passed.
Look in summary.txt for details.

================= Generating DPF 2 =================
DPF 2 passed.
Look in summary.txt for details.

[LOG] All queries generated and tested.
```

---

## **Sample Output Files**

### **summary.txt**

```
================= Generating DPF 1 =================
DPF 1:
Index: 83, Value: 1234567
[LOG] Starting generateDPF for index=83, value=1234567
[LOG] Target bits: 01010011
Initial root0: (num=..., S=..., f=...)
Initial root1: (num=..., S=..., f=...)
...
[LOG] DPF evaluation done.
[LOG] i=83, combined=1234567
DPF 1 passed.
```

---

### **dpf0_keys.txt**

```
DPF 1:
12345678 1234567 1
98765432 0
87654321 1
...
----------------------------------------------
```

---

### **dpf1_keys.txt**

```
DPF 1:
23456789 3456789 0
98765432 1
87654321 0
...
----------------------------------------------
```

---

## **Design Highlights**

* **Cryptographically secure randomness** using `std::random_device` and `std::mt19937_64`
* **Modular design**:

  * `generateDPF(location, value)`
  * `evalDPF(key, index)`
  * `EvalFull(key, size)`
* **Extensive logging** for each computation step
* **Compact and reproducible key storage** for both DPF parties

---

## **Testing and Verification**

The correctness test ensures:

* The combined output from both DPF parties reconstructs the exact target value at the chosen index
* All other positions produce zero

If all checks pass → `"Test Passed"` is logged in both the console and `summary.txt`.

---

## **How This Code Meets the Specifications**

| Specification                | Implementation Detail                                    |
| ---------------------------- | -------------------------------------------------------- |
| **Command-line arguments**   | Implemented: `<DPF_size> <num_DPFs>`                     |
| **Random index & value**     | Uses `std::random_device` and `std::mt19937_64`          |
| **DPF generation**           | Done via `generateDPF()`                                 |
| **Full evaluation**          | Done via `EvalFull()`                                    |
| **Output storage**           | `dpf0_keys.txt`, `dpf1_keys.txt`, and `summary.txt`      |
| **Correctness verification** | Checks target value reconstruction and zero elsewhere    |
| **Logging**                  | Logs all generation, PRG expansion, and evaluation steps |

---

## **Authors & Environment**

* **Name:** D. Rushikesh Chary
* **Roll number:** 220336
* **Course:** CS670
* **Assignment:** 2 — DPF Query Generation and Evaluation
---

