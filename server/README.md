### How to run the server

```bash
./build/bin/mcp_server <port> # default port is 8818
```

### SWitch Python Environment

```bash
rm -rf build

#example
cmake -DPython3_ROOT_DIR=/opt/anaconda3/envs/pytorch \
      -DPython3_INCLUDE_DIR=/opt/anaconda3/envs/pytorch/include/python3.9 \
      -DPython3_LIBRARY=/opt/anaconda3/envs/pytorch/lib/libpython3.9.dylib \
      -B build

# replace with your own python environment path
cmake -DPython3_ROOT_DIR=/path/to/your/python/environment \
      -DPython3_INCLUDE_DIR=/path/to/your/python/environment/include/python<version> \
      -DPython3_LIBRARY=/path/to/your/python/environment/lib/libpython<version>.dylib \
      -B build
```