# SecAgg-voleZK-mpc
This repo contains implementation of our scheme "Rhinobird: Efficient Integrity Defense for Secure Aggregation against Malicious Clients". The repository is built on \[[emp-toolkit](https://github.com/emp-toolkit)\] and \[[SIMC](https://github.com/shahakash28/simc)\].


# Installation
1. Clone this repo.
2. To install Eigen3 do:
   ```
   sudo apt-get update -y
   sudo apt-get install -y libeigen3-dev
   ```
3. Install SEAL 3.64:

   a. Clone \[[SEAL](https://github.com/microsoft/SEAL.git)\] repo in the parent directory `SecAgg-voleZK-mpc`.
   
   b. Execute 
   ```
   cd SEAL
   git checkout 3.6.4
   mkdir build && cd build
   cmake ..
   make -j
   sudo make install
   ```

# Compilation
1. In `SecAgg-voleZK-mpc`, go to `emp-tool` and do `git checkout df363bf30b56c48a12c352845efa3a4d8f75b388`.
2. Next, go to `emp-ot` in `SecAgg-voleZK-mpc` and do `git checkout 3b21d6314cb1e7d8dbb9bb1f1ed80261738e4f4c`.
3. For multi-threading support, go to `emp-tool` and run the following:
   ```
   cmake . -DTHREADING=ON
   make -j
   sudo make install
   ```
4. Do the same for emp-ot repository.
5. Finally, do the same in `Rhinobird` repository. (if any ".cmake" missing, find it in emp-toolkit)

# Run
Run the following test files from path `SecAgg-voleZK-mpc/Rhinobird`:

Examples:

```
EucSim:
Terminal 1: bin/test_fl_normball 1 0.0.0.0 31000 1 2048 100
Terminal 2: bin/test_fl_normball 2 127.0.0.1 31000 1 2048 100

CosSim:
Terminal 1: bin/test_fl_cossim 1 0.0.0.0 31000 1 1000 0
Terminal 2: bin/test_fl_cossim 2 127.0.0.1 31000 1 1000 0

L2check:
Terminal 1: bin/test_fl_l2check 1 0.0.0.0 31000 10 20 5
Terminal 2: bin/test_fl_l2check 2 127.0.0.1 31000 10 20 5

Lncheck:
Terminal 1: bin/test_fl_lncheck 1 0.0.0.0 31000 1000 1024
Terminal 2: bin/test_fl_lncheck 2 127.0.0.1 31000 1000 1024

Aggregation:
Terminal 1: bin/test_fl_aggregation 1 0.0.0.0 31000 1 1000000 1000
Terminal 2: bin/test_fl_aggregation 2 127.0.0.1 31000 1 1000000 1000
```



