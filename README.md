# Kernel Noodle
### Task
- Create a kernel module to access a kernel data structures of `vm_area_struct` and `inode`
- Create a user space program to use the kernel module and print a human-readable result
- User can pass console arguments to identify the structure (but not the direct address)
- Use the `/proc` filesystem to communicate between user and kernel space

### Kernel module
File named `noodle.c` is the whole module. It has to files to interact with the outside world:
1. `/proc/nq_noodle_vm_area`: accepts a pid and returns the first `vm_area_struct` of that process
2. `/proc/nq_noodle_inode`: accepts a file path and return the `inode` of that file

For a laugh, this kernel module uses JSON for data serialization

#### Possible error codes (write)
- `E2BIG`: too many arguments passed (`vm_area` only)
- `EINVAL`: error while parsing the arguments (all)
- `ESRCH`: process not found by pid (`vm_area` only)
- `EBUSY`: mutex is locked, a read must happen before write (all)

#### Possible error codes (read)
- `EFBIG`: not enough space to output the results
- `ENODATA`: no structures were fetched yet 
- `EFAULT`: error while copying data to user space

#### Running
```sh
./scrupt.sh  # compile and install the module

# Get inode information for `/tmp/something.txt`
echo "/tmp/something.txt" -n > /proc/nq_noodle_inode
cat /proc/nq_noodle_inode

# Get the first vm_area_struct of the init process
echo "1" -n > /proc/nq_noodle_vm_area
cat /proc/nq_noodle_vm_area
```

### User space program
File named `noodle.py`, CLI program written with [typer](https://typer.tiangolo.com/) using models from [pydantic](https://docs.pydantic.dev/) to read kernel module's JSONs

#### Usage
```sh
# Print out the help page
python noodle.py --help

# Fetch inode information for `/tmp/something.txt`
python noodle.py inode /tmp/something.txt

# Fetch the first vm_area_struct of the init process
python noodle.py vm_area 1
```
