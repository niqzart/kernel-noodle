// for linux 5.15.96-generic or equivalent

#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/namei.h>
#include <linux/path.h>
#include <linux/proc_fs.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("niqzart");

#define VM_AREA_FILENAME "nq_noodle_vm_area"
#define INODE_FILENAME "nq_noodle_inode"
#define BUFFER_LENGTH 1024

static char nq_buffer[BUFFER_LENGTH];
static unsigned long nq_buffer_size = 0;

static int read_buffer(const char __user *buffer, size_t len) {
    if (len > BUFFER_LENGTH) {
        nq_buffer_size = BUFFER_LENGTH;
    } else {
        nq_buffer_size = len;
    }

    if (copy_from_user(nq_buffer, buffer, nq_buffer_size)) {
        pr_alert("Error copying data from user\n");
        return -EFAULT;
    }
    if (nq_buffer_size != BUFFER_LENGTH) nq_buffer[nq_buffer_size] = '\0';

    return 0;
}

struct mutex nq_mutex_area_struct;
static struct vm_area_struct *vm_area_struct;

static ssize_t proc_vm_area_write(struct file *file, const char __user *buffer, size_t len,
                                  loff_t *offset) {
    int pid, error;
    struct task_struct *task;
    struct pid *found_pid;

    error = read_buffer(buffer, len);
    if (error) return error;

    if (sscanf(nq_buffer, "%d", &pid) != 1) {
        pr_alert("Error: wrong amount of arguments\n");
        return -E2BIG;
    }
    if (!pid) {
        pr_alert("Error: bad argument\n");
        return -EINVAL;
    }

    found_pid = find_get_pid(pid);
    if (!found_pid) {
        pr_alert("Error: process not found\n");
        return -ESRCH;
    }

    if (!mutex_trylock(&nq_mutex_area_struct)) {
        pr_alert("Error: can't acquire mutex\n");
        return -EBUSY;
    }

    task = get_pid_task(found_pid, PIDTYPE_PID);
    vm_area_struct = task->mm->mmap;

    return nq_buffer_size;
}

static ssize_t proc_vm_area_read(struct file *file, char __user *buffer, size_t len,
                                 loff_t *offset) {
    int length, inode_no;

    if (*offset > 0) return 0;

    if (len < BUFFER_LENGTH) {
        pr_alert("Buffer size too small\n");
        return -EFBIG;
    }

    if (vm_area_struct == NULL) {
        pr_alert("No vm_area_struct fetched\n");
        return -ENODATA;
    }

    if (vm_area_struct->vm_file == NULL) {
        inode_no = -1;
    } else {
        inode_no = vm_area_struct->vm_file->f_inode->i_ino;
    }

    length = sprintf(nq_buffer,
                     "{\"start\": %lu, \"end\": %lu, \"flags\": %lu, "
                     "\"prev\": %x, \"next\": %x, \"file_inode\": %d}\n",
                     vm_area_struct->vm_start, vm_area_struct->vm_end, vm_area_struct->vm_flags,
                     vm_area_struct->vm_prev != NULL, vm_area_struct->vm_next != NULL, inode_no);

    if (copy_to_user(buffer, nq_buffer, length)) {
        pr_alert("Error copying data to user\n");
        mutex_unlock(&nq_mutex_area_struct);
        return -EFAULT;
    }

    *offset += length;
    mutex_unlock(&nq_mutex_area_struct);
    return length;
}

struct mutex nq_mutex_inode;
static struct inode *inode;

static ssize_t proc_inode_write(struct file *file, const char __user *buffer, size_t len,
                                loff_t *offset) {
    struct path path;

    int error = read_buffer(buffer, len);
    if (error) return error;

    error = kern_path(nq_buffer, LOOKUP_FOLLOW, &path);
    if (error) {
        pr_alert("Error %d parsing path %s\n", error, nq_buffer);
        return -EINVAL;
    }

    if (!mutex_trylock(&nq_mutex_inode)) {
        pr_alert("Error: can't acquire mutex\n");
        return -EBUSY;
    }

    inode = path.dentry->d_inode;
    pr_debug("Successfully found inode: %lu for path name: %s\n", inode->i_ino, nq_buffer);

    return nq_buffer_size;
}

static ssize_t proc_inode_read(struct file *file, char __user *buffer, size_t len, loff_t *offset) {
    int length;

    if (*offset > 0) return 0;

    if (len < BUFFER_LENGTH) {
        pr_alert("Buffer size too small\n");
        return -EFBIG;
    }

    if (inode == NULL) {
        pr_alert("No inode fetched\n");
        return -ENODATA;
    }

    length = sprintf(nq_buffer,
                     "{\"no\": %lu, \"bytes\": %x, "
                     "\"atime\": %lld, \"mtime\": %lld, \"ctime\": %lld}\n",
                     inode->i_ino, inode->i_bytes, inode->i_atime.tv_sec, inode->i_mtime.tv_sec,
                     inode->i_ctime.tv_sec);

    if (copy_to_user(buffer, nq_buffer, length)) {
        pr_alert("Error copying data to user\n");
        mutex_unlock(&nq_mutex_inode);
        return -EFAULT;
    }

    *offset += length;
    mutex_unlock(&nq_mutex_inode);
    return length;
}

static const struct proc_ops vm_area_proc_ops = {
    .proc_read = proc_vm_area_read,
    .proc_write = proc_vm_area_write,
};

static const struct proc_ops inode_proc_ops = {
    .proc_read = proc_inode_read,
    .proc_write = proc_inode_write,
};

static struct proc_dir_entry *vm_area_proc_file;
static struct proc_dir_entry *inode_proc_file;

static int __init noodle_init(void) {
    vm_area_proc_file = proc_create(VM_AREA_FILENAME, 0666, NULL, &vm_area_proc_ops);
    if (vm_area_proc_file == NULL) {
        proc_remove(vm_area_proc_file);
        pr_alert("Error while creating proc file '%s'\n", VM_AREA_FILENAME);
        return -ENOMEM;
    }
    pr_debug("Proc file '%s' was created successfully\n", VM_AREA_FILENAME);

    inode_proc_file = proc_create(INODE_FILENAME, 0666, NULL, &inode_proc_ops);
    if (inode_proc_file == NULL) {
        proc_remove(inode_proc_file);
        pr_alert("Error while creating proc file '%s'\n", INODE_FILENAME);
        return -ENOMEM;
    }
    pr_debug("Proc file '%s' was created successfully\n", INODE_FILENAME);

    mutex_init(&nq_mutex_area_struct);
    mutex_init(&nq_mutex_inode);

    pr_debug("Noodle initialized successfully\n");

    return 0;
}

static void __exit noodle_exit(void) {
    proc_remove(vm_area_proc_file);
    pr_debug("Proc file '%s' was removed successfully\n", VM_AREA_FILENAME);
    proc_remove(inode_proc_file);
    pr_debug("Proc file '%s' was removed successfully\n", INODE_FILENAME);
}

module_init(noodle_init);
module_exit(noodle_exit);
