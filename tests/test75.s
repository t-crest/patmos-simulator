#
# Tests non-instruction bytes after disabled retnd result in error
#

                .word	100;
       (!p0)    retnd ;
                .word	2147483647;
                .word	2147483647;
                .word	2147483647;

