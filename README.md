# unshared-cli

**Deprecated** - Linux is not made for this, needs alot of twerking to get a stable environment

Scripts to provide a userland interface to for spawning containers
with `unshare(1)`, mainly to provide a clean way of running processes
in a isolated network namespace.

Linux specific and will, with a high probability, never be ported.

linux-3.8 or higher must be available to use `unshared-cli`.

To learn more about Linux namespaces see  `unshare(1)` and https://lwn.net/Articles/531114/

## Need hacks:

+ Use pivot_root instead of chroot
+ Make --pid and --user namespaces work


## Isolation levels (Namespaces and chroot's)

### Network NS

Provides separate interfaces, routing and what else you would expect 
to find in `/proc/net`. `unshared-cli` uses `brctl(8)` to make
connect the unshared process and host machine (I do suspect there are
more elegant ways).

### Pivot root and mount namespace

1. Find correct root dir
2. Mount / unpack chroot env
3. Enter unshare
4. do pre-mount
5. exec chroot < user expressions
6. do post-mount


#### thought on making chroot:
```bash
mount --make-rprivate /
mkdir -p <root>
mount -o tmpfs none <root>

cd <root>
gzip -dc /boot/initramfs-linux.img | cpio -i
```

##### Things needed to make sure everything run:
+ /etc/{passwd,group} (for whoami)
+ dynamic libraries:
  `for e in $cmds; do
		cp $(ldd $(command -v $e) | awk '/=> \//{print $3}) $target
	done`
+ possibly special needs?
 + whoami: `libnss{3,_,compat_files}.so*`
 + Needs `lo` interface for starting `erl`
 + libncurses.so.5 libtinfo.so.5 libcrypto.so.1.0.0.0 libz.so.1

### User NS

Isolates UID, GID's, capabilties and such. **note:** broke atm

### PID NS

Unique process space, forces the unshared process to have it's own
init process. 

### IPC & UTS NS

IPC provides separation of interprocess semaphores, shared memory and
message queues.

UTS opens for individual host/domain names for each unshared process.

## Example

```bash
# Define a chroot type
UROOT=$(unshared-cli get-root erlang)
mkdir -p $UROOT && cd $UROOT
gzip -dc /boot/initramfs-linux.img | cpio -i
# Copy required library files, and scripts
cp -v \
	$(ldd $(command -v /usr/lib/erlang/bin/escript) | awk '/=> \//{print $3}') \
	$UROOT/usr/lib
cp $(command -v whoami) !$
```

```bash
# Run an isolated process in an erlang capable chroot
# --mount gets passed to unshare
#
# --root just specifies the 'type' of chroot to use, this must be
# pre-generated by YOU. --data-dir A:B specifies a mount, if A's
# filetype equals 'directory' it will use the 'bind' option.
unshared-cli spawn \
	--mount \
	--root erlang \
	--name riak-01 \
	--data-dir /var/data/riak-01:/var
	--exec "/var/bin/riak foreground"

# The above roughly expands to
# sudo unshare -m
# ROOTDIR=$(unshared-cli get-root erlang)
# DATADIR=$ROOTDIR/var
# mount --make-private -o bind /var/data/riak-01 $DATADIR
# chroot $ROOTDIR /bin/sh
# /var/bin/riak foreground
```

```bash
# Run an process with it's own network stack.
# --net and --uts gets passed to unshare
#
# --exec can be passed multiple times and will be evaluated in order.
# note: --exec operations are NOT atomic.
unshared-cli spawn
	--net --uts \
	--name netcat \
	--exec "ip link set int3 netns $$; ifconfig int3 172.3.0.2/24"
	--exec "netcat -l 0.0.0.0 7666"

# The above roughly expands to
# sudo unshare  -n
# ip link set int3 netns $$
# ifconfig int3 172.3.0.2/24
# netcat -l 0.0.0.0 7666
```

```bash
# Use `initnetns` to handle setup of network namespace
./unshared-cli spawn \
	--uts --ipc \
	--preexec utils/initnetns mybr \$NAME 4.5.6.7/24 \
	--chroot utils/forkn \'echo die\' /bin/ip netns exec \$NAME chroot \
	--postexec utils/destroynetns mybr \$NAME
	-- /bin/netcat -l 7001
