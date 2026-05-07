
Examples:

```
fl_normball.cpp:
Terminal 1: bin/test_fl_normball 1 0.0.0.0 31000 1
Terminal 2: bin/test_fl_normball 2 127.0.0.1 31000 1
Terminal 1: bin/test_fl_normball 1 0.0.0.0 31000 1 2048 100
Terminal 2: bin/test_fl_normball 2 127.0.0.1 31000 1 2048 100

fl_cossim.cpp:
Terminal 1: bin/test_fl_cossim 1 0.0.0.0 31000 1 1000 0
Terminal 2: bin/test_fl_cossim 2 127.0.0.1 31000 1 1000 0
Terminal 1: bin/test_fl_normball 1 0.0.0.0 31000 1
Terminal 2: bin/test_fl_normball 2 127.0.0.1 31000 1

fl_l2check.cpp:
Terminal 1: bin/test_fl_l2check 1 0.0.0.0 31000 10 20 5
Terminal 2: bin/test_fl_l2check 2 127.0.0.1 31000 10 20 5
Terminal 1: bin/test_fl_l2check 1 0.0.0.0 31000 10 20 17
Terminal 2: bin/test_fl_l2check 2 127.0.0.1 31000 10 20 17

fl_lncheck.cpp:
Terminal 1: bin/test_fl_lncheck 1 0.0.0.0 31000 10 20
Terminal 2: bin/test_fl_lncheck 2 127.0.0.1 31000 10 20
Terminal 1: bin/test_fl_lncheck 1 0.0.0.0 31000 1000 1024
Terminal 2: bin/test_fl_lncheck 2 127.0.0.1 31000 1000 1024

fl_mult.cpp:
Terminal 1: bin/test_fl_mult 1 0.0.0.0 31000 8193 1 0
Terminal 2: bin/test_fl_mult 2 127.0.0.1 31000 8193 1 0
Terminal 1: bin/test_fl_mult 1 0.0.0.0 31000 100000 1 1
Terminal 2: bin/test_fl_mult 2 127.0.0.1 31000 100000 1 1

fl_comp.cpp:
Terminal 1: bin/test_fl_comp 1 0.0.0.0 31000 1 59 1 1
Terminal 2: bin/test_fl_comp 2 127.0.0.1 31000 1 59 1 1
Terminal 1: bin/test_fl_comp 1 0.0.0.0 31000 100 59 1 1
Terminal 2: bin/test_fl_comp 2 127.0.0.1 31000 100 59 1 1

fl_aggregation.cpp:
Terminal 1: bin/test_fl_aggregation 1 0.0.0.0 31000 1 10 10
Terminal 2: bin/test_fl_aggregation 2 127.0.0.1 31000 1 10 10
Terminal 1: bin/test_fl_aggregation 1 0.0.0.0 31000 0 10000 5
Terminal 2: bin/test_fl_aggregation 2 127.0.0.1 31000 0 10000 5
Terminal 1: bin/test_fl_aggregation 1 0.0.0.0 31000 1 1000000 1000
Terminal 2: bin/test_fl_aggregation 2 127.0.0.1 31000 1 1000000 1000
```

