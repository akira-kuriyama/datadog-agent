#ifndef _MKDIR_H_
#define _MKDIR_H_

struct mkdir_event_t {
    struct event_t event;
    struct process_data_t process;
    int    inode;
    u32    pathname_key;
    int    mount_id;
    int    mode;
};

int __attribute__((always_inline)) trace__security_inode_mkdir(struct pt_regs *ctx, struct inode *dir, struct dentry *dentry, umode_t mode) {
    struct dentry_event_cache_t cache = {
        .src_dentry = dentry,
        .src_dir = dir,
        .mode = mode,
    };

    // Filter process
    fill_event_context(&cache.event_context);
    if (!filter(&cache.event_context))
        return 0;

    push_dentry_event_cache(&cache);

    return 0;
}

SEC("kprobe/security_inode_mkdir")
int kprobe__security_inode_mkdir(struct pt_regs *ctx) {
    struct inode *dir = (struct inode *)PT_REGS_PARM1(ctx);
    struct dentry *dentry = (struct dentry *)PT_REGS_PARM2(ctx);
    umode_t mode = (umode_t)PT_REGS_PARM3(ctx);

    return trace__security_inode_mkdir(ctx, dir, dentry, mode);
}

int __attribute__((always_inline)) trace__security_inode_mkdir_ret(struct pt_regs *ctx, int retval) {
    struct dentry_event_cache_t *cache = pop_dentry_event_cache();
    if (!cache)
        return -1;

    struct mkdir_event_t event = {
        .event.retval = retval,
        .event.type = EVENT_VFS_MKDIR,
        .event.timestamp = bpf_ktime_get_ns(),
        .pathname_key = bpf_get_prandom_u32(),
        .inode = get_dentry_inode(cache->src_dentry),
        .mount_id = get_inode_mount_id(cache->src_dir),
        .mode = cache->mode,
    };

    fill_process_data(&event.process);
    resolve_dentry(cache->src_dentry, event.pathname_key);

    send_event(ctx, event);

    return 0;
}

SEC("kretprobe/security_inode_mkdir")
int kretprobe__security_inode_mkdir(struct pt_regs *ctx) {
    return trace__security_inode_mkdir_ret(ctx, PT_REGS_RC(ctx));
}

#endif