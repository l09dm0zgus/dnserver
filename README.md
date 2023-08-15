# DNS Server

Simple DNS server written in C.

Based on this [videos](https://www.youtube.com/watch?v=HdrPWGZ3NRo&list=PLBOh8f9FoHHhvO5e5HF_6mYvtZegobYX2&index=1)

Implementation of Thread Pool in C - https://nachtimwald.com/2019/04/12/thread-pool-in-c/

## Dependencies
* POSIX Threads
* POSIX Sockets
* JSON-C [download here](https://github.com/json-c/json-c.git) or just clone this repository recursively.

## Building

```bash
cd <path to cloned repo>
mkdir build 
cp zones build
cmake ..
make 
```

## Server Configuration

Configurations stored in directory zones.

Zones files is json.

For adding a new zone add file <zone name>.zone .

Copy content from file template.zone to created file.

Change origin to your domain , value to address.

For adding a domain to block list,add a domain to end of file blacklist , and change address to redirect in noresolved.zone.

For redirecting DNS client  to another server set port and ip address in file redirect.conf(default ip : 8.8.8.8 , port : 53) .
