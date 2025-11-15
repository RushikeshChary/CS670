# CS670 Assignment 1 : Denchanadula Rushikesh Chary (220336)

## Introduction

In this assignment, two parties P0 and P1 are given secret shares of the matrices:

$$
U \in \mathbb{R}^{m \times k}, \quad V \in \mathbb{R}^{n \times k}
$$

Here, $m$ is the number of users, $n$ is the number of items, and $k$ is the number of features. Each party receives $q$ queries, where each query specifies a user $i$ and additive shares of an item $j$.

For each query, the goal is to update the $i$-th row of $U$ securely using the formula:

$$
u_i \gets u_i + v_j \cdot (1 - \langle u_i, v_j \rangle)
$$

We implement this using a secure multiparty computation (MPC) framework.

---

## Methodology
The shares of $V$ are stored in an **ORAM** supporting read-only access to prevent leakage of access patterns.
### Step 1: Query Generation (`gen_queries.cpp`)

* Take input parameters `m, n, k` from the command line.
* Generate two files (say `f1` and `f2`) containing:

  * Values of `m, n, k, Q`.
  * Next `Q` lines: queries of the form `{i, j0}` and `{i, j1}`.
* Parties `P0` and `P1` read these files and independently create random shares of `U0, V0` and `U1, V1`.
---
### Step 2: Rotation Trick with P2

* P2 sends `k` and shares of `e_k` to both parties.
* For each query `{(i, j0), (i, j1)}`:

  1. Compute `i_b - k` and exchange shares.
  2. Add received share to get `(i - k)`.
  3. Rotate vector shares to obtain standard basis vector shares of `e_i`.

---

### Step 3: Blinding of Database Shares

* Each party (`P0`, `P1`) chooses a random matrix (`r0`, `r1`) of same size as `V`.
* Blind their share of `V` and exchange with the other party.
* After re-blinding with their own chosen random matrix, both parties obtain a **DORAM read environment**.

---

### Step 4: Retrieval of `V_j`

* Parties already have vector shares of `e_j` (say `f1`, `f2`).
* Compute dot products:

  * `P0`: `<D + r0 + r1, f1>`
  * `P1`: `<D + r0 + r1, f2>`
* Result: shares of `D[j] + r0[j] + r1[j]`.
* To unmask:

  * Compute shares of `r0[j] + r1[j]` using Du-Atallah dotproduct.
  * Subtract to obtain final shares of `D[j]`.

---

### Step 5: Update of `U_i`

* Both parties know `i`, so they retrieve `u_i` from their shares.
* Compute delta `z`:

  * `z1' = <u_i1, v_j1>`
  * `z = 1 - <u_i1, v_j1>`
* Update shares of `u_i`:

  * Perform scalar multiplication of `v_j` with `z` using Du-Atallah.
  * Compute new share: `u_i' = u_i - v_j * z`.

---

### How to Run

* **Generate Queries:** `gen_queries.cpp` generates two files, `q0.txt` and `q1.txt`, containing queries for P0 and P1, respectively.

```bash
g++ gen_queries.cpp -o gen_queries
./gen_queries m n k Q
```
where, m, n, k, Q are the above mentioned parameters
* **Run Protocol:** The Docker environment simulates the three-party setup: P0 and P1 perform the computations, while P2 acts as a helper providing common values.

```bash
docker-compose build
docker-compose up   # start the protocol
```
* **Debuggig output:** I have implemented a logger in my code where the parties/servers will be loggin their shares of various values a text file. But since the files are stored in the docker's cloud environment, it's not reflected in local view of these files. So we need to explicitly copy them back to our local environment. The following commands help in that case.
```bash
docker cp p0:/app/o1.txt ./o1.txt
docker cp p1:/app/o2.txt ./o2.txt
```
* **Check Correctness:** I have written a simple output checker... Given initial setup (in a text file (out.txt)), i.e., query of both parties and thier starting values of U and V matrices, and final updated U matrix, it will determaine whether the updation is correct or not. In here too, it will log the values like old matrices and expected matrices in a file - check.txt.
```bash
./checker out.txt
```
---

### Security

* **ORAM Read:** Prevents leakage of item indices when accessing $V$.
* **Du-Atallah Protocol:** Enables secure computation of dot products and masked multiplications without revealing shares.
* **Privacy:** The protocol ensures that neither P0 nor P1 learns the original shares of the other party or the values of the third party.

---

### Communication Efficiency

* **ORAM Read:** Retrieving a masked row of $V$ requires $\mathcal{O}(1)$ communication via rotation, followed by $\mathcal{O}(nk)$ for mask removal.
* **Secure Dot Product:** Computing $\langle u_i, v_j \rangle$ requires $\mathcal{O}(k)$ communication.
* **Du-Atallah Updates:** Updating $u_i$ with $v_j \cdot \delta$ involves $k$ executions of the Du-Atallah protocol, resulting in $\mathcal{O}(k)$ communication.

Overall, the protocol securely updates the user matrix while keeping communication costs low.

---
## Note
Code has comments almost at all places... Please refer to it for further details.