This directory contains a FastRPC ioctl wrapper and a reverse tunnel.

FastRPC is used to communicate with the Context Hub Runtime Environment, a
program on the DSP that manages sensors, and to serve files to remote
processors.

# Wrapper function

The wrapper function is designed so that the ioctl arguments do not need to be
constructed for each remote method. Instead, it accepts an open file descriptor,
handle, method definition, and arguments to the remote method using `va_args`.
This makes it relatively easy to add remote methods without the use of the QAIC
interface compiler or a free software alternative.

## Installation

There is currently no build system.

    $ gcc -o libhexagonrpc.so -shared fastrpc.c context.c

## Usage

Making already-defined method calls is relatively straightforward, depending on
the remote method that you're calling. You just need the function definition, an
open file descriptor, the handle, and the arguments for it.

    ret = fastrpc2(&adsp_listener_next2_def, fd, ADSP_LISTENER_HANDLE,
    	       prev_ctx,
    	       prev_result,
    	       nested_outbufs_len, nested_outbufs,
    	       ctx,
    	       nested_handle,
    	       nested_sc,
    	       nested_inbufs_len,
    	       nested_inbufs_size, nested_inbufs);

All inputs are specified before all outputs. Words in the first input and output
buffers go in the form of `uint32_t` arguments. Each buffer is accepted as a
`uint32_t` length and a pointer.

### Creating function definitions

Assuming you already have knowledge about the remote method to call, you must
first write a function definition for it. A function definition is a struct with
information on its method ID and the arguments it accepts.

First, the method ID is taken from the first argument of the
`REMOTE_METHOD_MAKE()` macro or the second argument of the
`REMOTE_METHOD_MAKEX()` macro.

Example:

    REMOTE_SCALARS_MAKE(4, 2, 2)

has the method 4.

Next, information on how many arguments it takes must be specified. There are
four types of arguments:

- 32-bit input words in the first input buffer, excluding lengths for the input
  and output buffers
- 32-bit output words in the first output buffer
- input buffers, excluding the first
- output buffers, excluding the first

To get the size of the first input and output buffers, a dump must be consulted,
or the prim argument of generated C files:

    invoking via fastrpc, handle 3, method 4, 2 inbufs, 2 outbufs, 0 inhandles, 0 outhandles
    fastrpc argument 00000000FFFFFFFF0000000000000000 // 16 bytes, or 4 words
    fastrpc argument 
    invoking via fastrpc, handle 5, method 4, 1 inbufs, 1 outbufs, 0 inhandles, 0 outhandles
    fastrpc return 91000000000000000002020029000000 // 16 bytes, or 4 words
    fastrpc return 

Since there are 2 input buffers excluding the first (these can have a variable
size), 2 must be subtracted from the 4 input words. These will be provided by
the wrapper as the sizes for the variable-length input and output buffers.

Counting each of these will give the numbers needed for the function definition.

A function definition looks like:

    const struct fastrpc_function_def_interp2 adsp_listener_next2_def = {
    	.msg_id = 4,
    	.in_nums = 2,
    	.in_bufs = 1,
    	.out_nums = 4,
    	.out_bufs = 1,
    };

or:

    #define DEFINE_REMOTE_PROCEDURE(mid, name,				\
    				innums, inbufs,				\
    				outnums, outbufs)			\
    	const struct fastrpc_function_def_interp2 name##_def = {	\
    		.msg_id = mid,						\
    		.in_nums = innums,					\
    		.in_bufs = inbufs,					\
    		.out_nums = outnums,					\
    		.out_bufs = outbufs,					\
    	};

    DEFINE_REMOTE_PROCEDURE(4, adsp_listener_next2, 2, 1, 4, 1)

# Reverse tunnel

The reverse tunnel calls the `adsp_listener_next2` remote method to receive
method calls for the Application Processor.

Interfaces are listed in the `fastrpc_listener_interfaces` variable.

## HexagonFS

The reverse tunnel's `apps_std` interface serves files to the remote processor.
These files are searched for in:

    Physical file/dir			Android file/dir
    /etc/qcom/sns_reg.conf			/vendor/etc/sensors/sns_reg_config
    /etc/qcom/sensors.d			/vendor/etc/sensors/config
    /var/lib/qcom/sensors			/mnt/vendor/persist/sensors/registry
    /usr/lib/qcom/adsp			/vendor/dsp/adsp
    /usr/lib/qcom/adsp/avs			/vendor/etc/acdbdata

These files and directories should be populated with files from your device's
Android firmware.

# Future plans

This directory is a bit messy. It should be organized into different
subdirectories and have a build system. This should also be renamed to
HexagonRPC to avoid a name collision.

Reverse tunnels are separated by the process that opens the file descriptor and
cannot service requests from other processes that do not share the same file
descriptor. There should be some daemon that opens the device and allows clients
to send requests.

FastRPC may be the way to offload work to the DSPs. When a FastRPC function call
is made, the `<name>_skel_invoke` function in a shared object named
`lib<name>_skel.so` is called on the DSP. Further investigation is needed to
make a working build system for this.
