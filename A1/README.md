## CS670 Assignment 1 : Denchanadula Rushikesh Chary (220336)

### Introduction

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

### Methodology

The shares of $V$ are stored in an **ORAM** supporting read-only access to prevent leakage of access patterns. For each query:

1. After obtaining shares of $j$, we perform an oblivious read of $V$ to retrieve shares of $v_j$. (These are obtained using rotation trick)
2. Using a mask and a modified **Du-Atallah protocol**, parties compute shares of $\langle r_0 + r_1, e_j \rangle$ (where $r_0$ and $r_1$ are the masks of P0 and P1) and subtract it from the masked $v_j$ to reconstruct the original shares of $v_j$.
3. The parties then securely compute $\langle u_i, v_j \rangle$ using the modified Du-Atallah protocol.
4. Next, they compute $\delta = 1 - \langle u_i, v_j \rangle$ in secret-shared form.
5. Finally, they compute $v_j \cdot \delta$ using the Du-Atallah protocol $k$ times and add it to the shares of $u_i$ to update the matrix.


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