### /etc/resolv.conf
```
nameserver 10.123.119.98
nameserver 10.123.120.110
```

### default
./resolver-demo -host=www.qq.com
tcpdump -ieth1 -w cares-www.qq.com-default.pcap host 10.123.119.98 or host 10.123.120.110

### use tcp
./resolver-demo -host=www.qq.com -use_tcp
tcpdump -ieth1 -w cares-www.qq.com-tcp.pcap host 10.123.119.98 or host 10.123.120.110

### public dns server
./resolver-demo -host=www.qq.com -use_tcp -servers=8.8.8.8,8.8.4.4
./resolver-demo -host=www.qq.com -use_tcp -servers=8.8.4.4,8.8.8.8

tcpdump -ieth1 -w cares-www.qq.com-8.8.8.8.pcap host 8.8.8.8 or host 8.8.4.4
tcpdump -ieth1 -w cares-www.qq.com-8.8.4.4.pcap host 8.8.8.8 or host 8.8.4.4

### test timeout
./resolver-demo -host=www.qq.com -timeout=1000 -servers=8.8.4.4,8.8.8.8

### parallel resolve
./dns_test --gtest_filter=DnsTest.CallbackLeakTest
tcpdump -ieth1 -w cares-parallel-cancel.pcap host 10.123.119.98 or host 10.123.120.110 or host 8.8.8.8

./dns_test --gtest_filter=DnsTest.ParallelResolveTest
tcpdump -ieth1 -w cares-parallel-done.pcap host 10.123.119.98 or host 10.123.120.110 or host 8.8.8.8
