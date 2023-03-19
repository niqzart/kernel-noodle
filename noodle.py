from datetime import datetime
from os import system
from pathlib import Path
from time import sleep
from typing import Optional

from pydantic import BaseModel, validator, Field
from typer import Typer

app = Typer()

data_path: Path = Path("tmp.txt")
error_path: Path = Path("tmp.err")


def system_safe(command: str) -> None:
    res = system(command + " 2> /dev/null")
    if res:
        system("dmesg | tail -1 > tmp.err")
        with error_path.open() as f:
            error = f.read()
        error_path.unlink()
        raise Exception(f"Error occurred: {error}")


def system_retry(command: str, retries: int, wait: float) -> None:
    for i in range(retries):
        try:
            system_safe(command)
            return
        except Exception as e:
            if "Error: can't acquire mutex" not in e.args[0]:
                raise e
            if i == retries - 1:
                raise e
            sleep(wait)


def datetime_converter(var: str):
    @validator(var, pre=True, allow_reuse=True)
    def convert_time(cls, value: int) -> datetime:
        return datetime.fromtimestamp(value)

    return convert_time


class Inode(BaseModel):
    no: int
    bytes: int
    access_time: datetime = Field(alias="atime")
    modification_time: datetime = Field(alias="mtime")
    creation_time: datetime = Field(alias="ctime")

    validator_0 = datetime_converter("access_time")
    validator_1 = datetime_converter("modification_time")
    validator_2 = datetime_converter("creation_time")


@app.command("inode")
def get_inode(filepath: Path) -> None:
    system_retry(f"echo -n '{filepath}' > /proc/nq_noodle_inode", 10, 0.2)
    system_safe(f"cat /proc/nq_noodle_inode > tmp.txt")
    inode = Inode.parse_file("tmp.txt")
    data_path.unlink()

    print(f"Inode for {filepath}")
    print(f"No: {inode.no}")
    print(f"Size: {inode.bytes}")
    print(f"Created: {inode.creation_time}")
    print(f"Modified: {inode.modification_time}")
    print(f"Accessed: {inode.access_time}")


def bool_converter(var: str):
    @validator(var, pre=True, allow_reuse=True)
    def convert_bool(cls, value: int) -> bool:
        return value != 0

    return convert_bool


class VMArea(BaseModel):
    start: int
    end: int
    flags: int
    prev: bool
    next: bool
    file_inode: Optional[int]

    validator_0 = bool_converter("prev")
    validator_1 = bool_converter("next")

    @validator("file_inode", pre=True)
    def convert_inode(cls, value: int) -> Optional[int]:
        if value == -1:
            return None
        return value


@app.command("vm_area")
def get_vm_area(pid: int) -> None:
    system_retry(f"echo -n '{pid}' > /proc/nq_noodle_vm_area", 10, 0.1)
    system_safe(f"cat /proc/nq_noodle_vm_area > tmp.txt")
    vm_area = VMArea.parse_file("tmp.txt")
    data_path.unlink()

    print(f"VM area for pid={pid}")
    print(f"Start: {vm_area.start}")
    print(f"End: {vm_area.end}")
    print(f"Flags: {vm_area.flags}")
    print(f"Has previous: {vm_area.prev}")
    print(f"Has next: {vm_area.next}")
    if vm_area.file_inode is None:
        print("File: nothing attached")
    else:
        print(f"File: {vm_area.file_inode}")


if __name__ == "__main__":
    app()
