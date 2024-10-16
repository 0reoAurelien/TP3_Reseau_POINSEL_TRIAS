#include <fcntl.h>
#include <sys/mman.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "tosfs.h"
#include <fuse/fuse_lowlevel.h>
#include <errno.h>
#include <stdlib.h>
#include <assert.h>



static const char *hello_str = "Hello World!\n";
static const char *file_name = "test_tofsf_files";

static int stat_3is(fuse_ino_t ino, struct stat *stbuf)
{
	stbuf->st_ino = ino;
	switch (ino) {
	case 1:
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
		break;

	case 2:
		stbuf->st_mode = S_IFREG | 0444;
		stbuf->st_nlink = 1;
		stbuf->st_size = strlen(file_name);
		break;

	default:
		return -1;
	}
	return 0;
}

static void getattr_3is(fuse_req_t req, fuse_ino_t ino,struct fuse_file_info *fi)
{
	struct stat stbuf;

	(void) fi;

	memset(&stbuf, 0, sizeof(stbuf));
	if (stat_3is(ino, &stbuf) == -1)
		fuse_reply_err(req, ENOENT);
	else
		fuse_reply_attr(req, &stbuf, 1.0);
}

static void lookup_3is(fuse_req_t req, fuse_ino_t parent, const char *name)
{
	struct fuse_entry_param e;

	if (parent != 1 || strcmp(name, file_name) != 0)
		fuse_reply_err(req, ENOENT);
	else {
		memset(&e, 0, sizeof(e));
		e.ino = 2;
		e.attr_timeout = 1.0;
		e.entry_timeout = 1.0;
		stat_3is(e.ino, &e.attr);

		fuse_reply_entry(req, &e);
	}
}

struct dirbuf {
	char *p;
	size_t size;
};

static void dirbuf_add(fuse_req_t req, struct dirbuf *b, const char *name,
		       fuse_ino_t ino)
{
	struct stat stbuf;
	size_t oldsize = b->size;
	b->size += fuse_add_direntry(req, NULL, 0, name, NULL, 0);
	b->p = (char *) realloc(b->p, b->size);
	memset(&stbuf, 0, sizeof(stbuf));
	stbuf.st_ino = ino;
	fuse_add_direntry(req, b->p + oldsize, b->size - oldsize, name, &stbuf,
			  b->size);
}

#define min(x, y) ((x) < (y) ? (x) : (y))

static int reply_buf_limited(fuse_req_t req, const char *buf, size_t bufsize,
			     off_t off, size_t maxsize)
{
	if (off < bufsize)
		return fuse_reply_buf(req, buf + off,
				      min(bufsize - off, maxsize));
	else
		return fuse_reply_buf(req, NULL, 0);
}

static void readdir_3is(fuse_req_t req, fuse_ino_t ino, size_t size,
			     off_t off, struct fuse_file_info *fi)
{
	(void) fi;

	if (ino != 1)
		fuse_reply_err(req, ENOTDIR);
	else {
		struct dirbuf b;

		memset(&b, 0, sizeof(b));
		dirbuf_add(req, &b, ".", 1);
		dirbuf_add(req, &b, "..", 1);
		dirbuf_add(req, &b, file_name, 2);
		reply_buf_limited(req, b.p, b.size, off, size);
		free(b.p);
	}
}

static void open_3is(fuse_req_t req, fuse_ino_t ino,
			  struct fuse_file_info *fi)
{
	if (ino != 2)
		fuse_reply_err(req, EISDIR);
	else if ((fi->flags & 3) != O_RDONLY)
		fuse_reply_err(req, EACCES);
	else
		fuse_reply_open(req, fi);
}

static void read_3is(fuse_req_t req, fuse_ino_t ino, size_t size,
			  off_t off, struct fuse_file_info *fi)
{
	(void) fi;

	assert(ino == 2);
	reply_buf_limited(req, hello_str, strlen(hello_str), off, size);
}

static struct fuse_lowlevel_ops oper_3is = {
	.lookup		= lookup_3is,
	.getattr	= getattr_3is,
	.readdir	= readdir_3is,
	.open		= open_3is,
	.read		= read_3is,
};

int main(int argc, char *argv[]) {

	int fd = open(argv[1], O_RDWR);
	if (fd == -1) {
		perror("open");
		return 1;
	}

	// Utilisation de mmap pour mapper le fichier dans la mémoire
	void *mapped_fs = mmap(NULL, TOSFS_BLOCK_SIZE * 32, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (mapped_fs == MAP_FAILED) {
		perror("mmap");
		close(fd);
		return 1;
	}
	struct tosfs_superblock *sb = mapped_fs;

	struct stat * size = malloc(sizeof(struct stat));
	stat_3is(sb->inodes,size);
	printf("%i \n",size->st_mode);

	/*struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
	struct fuse_chan *ch;
	char *mountpoint;
	int err = -1;

	if (fuse_parse_cmdline(&args, &mountpoint, NULL, NULL) != -1 &&
		(ch = fuse_mount(mountpoint, &args)) != NULL) {
		struct fuse_session *se;

		se = fuse_lowlevel_new(&args, &oper_3is,sizeof(oper_3is), NULL);
		if (se != NULL) {
			if (fuse_set_signal_handlers(se) != -1) {
				fuse_session_add_chan(se, ch);
				err = fuse_session_loop(se);
				fuse_remove_signal_handlers(se);
				fuse_session_remove_chan(ch);
			}
			fuse_session_destroy(se);
		}
		fuse_unmount(mountpoint);
		}
	fuse_opt_free_args(&args);
*/
	return 0;
}



 /*  if (argc != 2) {
        fprintf(stderr, "Usage: %s <filesystem image file>\n", argv[0]);
        return 1;
    }

    // Ouvrir le fichier mit en argument
    int fd = open(argv[1], O_RDWR);
    if (fd == -1) {
        perror("open");
        return 1;
    }

    // Utilisation de mmap pour mapper le fichier dans la mémoire
    void *mapped_fs = mmap(NULL, TOSFS_BLOCK_SIZE * 32, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (mapped_fs == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return 1;
    }

    // Récupérer le superblock à l'adresse mappée
    struct tosfs_superblock *sb = mapped_fs;




    // Afficher les informations sur le superblock
    printf("Filesystem Information:\n");
    printf("Magic number: 0x%02x\n", sb->magic);
    // Vérifier le numéro magique
    if (sb->magic != TOSFS_MAGIC) {
        fprintf(stderr, "Invalid filesystem magic number: 0x%x\n", sb->magic);
        munmap(mapped_fs, TOSFS_BLOCK_SIZE * 32);
        close(fd);
        return 1;
    }
    printf("Block bitmap: 0x%02x\n", sb->block_bitmap);
    printf(PRINTF_BINARY_PATTERN_INT32,PRINTF_BYTE_TO_BINARY_INT32(sb->block_bitmap));
    printf("\nInode bitmap: 0x%02x\n", sb->inode_bitmap);
    printf(PRINTF_BINARY_PATTERN_INT32,PRINTF_BYTE_TO_BINARY_INT32(sb->inode_bitmap));
    printf("\nBlock size: %u\n", sb->block_size);
    printf("Total blocks: %u\n", sb->blocks);
    printf("Total inodes: %u\n", sb->inodes);
    printf("Root inode: %u\n", sb->root_inode);

    // Nettoyage
    munmap(mapped_fs, TOSFS_BLOCK_SIZE * 32);
    close(fd);

    return 0;*/

