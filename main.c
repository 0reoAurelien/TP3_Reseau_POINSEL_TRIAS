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

static void getattr_3is(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi) {
	struct stat stbuf;

	(void) fi;

	memset(&stbuf, 0, sizeof(stbuf));
	if (stat_3is(ino, &stbuf) == -1) {
		fuse_reply_err(req, ENOENT);
	} else {
		fuse_reply_attr(req, &stbuf, 1.0);
	}
}


static void lookup_3is(fuse_req_t req, fuse_ino_t parent, const char *name) {
	struct fuse_entry_param e;

	if (parent != 1 || strcmp(name, file_name) != 0) {
		fuse_reply_err(req, ENOENT);
	} else {
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

static void readdir_3is(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off, struct fuse_file_info *fi) {
	(void) fi;

	if (ino != 1) {
		fuse_reply_err(req, ENOTDIR);
	} else {
		struct dirbuf b;
		memset(&b, 0, sizeof(b));
		dirbuf_add(req, &b, ".", 1);       // Répertoire courant
		dirbuf_add(req, &b, "..", 1);      // Parent
		dirbuf_add(req, &b, file_name, 2); // Fichier présent
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

static void read_3is(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off, struct fuse_file_info *fi) {
	(void) fi;

	assert(ino == 2);
	reply_buf_limited(req, hello_str, strlen(hello_str), off, size);
}

static void create_3is(fuse_req_t req, fuse_ino_t parent, const char *name, mode_t mode, struct fuse_file_info *fi) {
	if (parent != 1 || strcmp(name, file_name) != 0) {
		fuse_reply_err(req, EEXIST);
	} else {
		struct fuse_entry_param e;
		memset(&e, 0, sizeof(e));
		e.ino = 3;  // Numéro d'inode du nouveau fichier
		e.attr_timeout = 1.0;
		e.entry_timeout = 1.0;
		stat_3is(e.ino, &e.attr);
		fuse_reply_create(req, &e, fi);
	}
}

static void write_3is(fuse_req_t req, fuse_ino_t ino, const char *buf, size_t size, off_t off, struct fuse_file_info *fi) {
	(void) fi;

	if (ino != 2) {
		fuse_reply_err(req, EISDIR);
	} else {
		// Écrire dans le fichier (simuler l'écriture)
		// Vous pouvez ajouter une logique pour modifier le contenu du fichier
		fuse_reply_write(req, size);
	}
}

static struct fuse_lowlevel_ops oper_3is = {
	.lookup     = lookup_3is,
	.getattr    = getattr_3is,
	.readdir    = readdir_3is,
	.open       = open_3is,
	.read       = read_3is,
	.create     = create_3is,
	.write      = write_3is,
};

void test_getattr(const char *file) {
	struct stat st;
	if (stat("test_tosfs_files", &st) == -1){
		perror("stat");
	} else {
		printf("File: %s\n", file);
		printf("File size: %ld\n", st.st_size);
		printf("Number of links: %ld\n", st.st_nlink);
		printf("File mode: %o\n", st.st_mode);
	}
}

void test_lookup(const char *file) {
	if (access(file, F_OK) == 0) {
		printf("File %s exists.\n", file);
	} else {
		printf("File %s does not exist.\n", file);
	}
}

/*int main(int argc, char *argv[]) {
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
	struct fuse_chan *ch;
	char *mountpoint;
	int err = -1;
	if (fuse_parse_cmdline(&args, &mountpoint, NULL, NULL) != -1 && (ch = fuse_mount(mountpoint, &args)) != NULL) {
		struct fuse_session *se;
		se = fuse_lowlevel_new(&args, &oper_3is, sizeof(oper_3is), NULL);

		if (se != NULL) {
			if (fuse_set_signal_handlers(se) != -1) {
				fuse_session_add_chan(se, ch);

				test_getattr("test_tosfs_files");
				test_lookup("test_tosfs_files");

				err = fuse_session_loop(se);
				fuse_remove_signal_handlers(se);
				fuse_session_remove_chan(ch);
			}
			fuse_session_destroy(se);
		}
		fuse_unmount(mountpoint);
	}

	fuse_opt_free_args(&args);
	return err ? 1 : 0;
}*/

//petit main pour tester getattr et lookup car le main au dessus ne fonctionne pas
int main(int argc, char *argv[]) {
	test_getattr("test_tosfs_files");
	test_lookup("test_tosfs_files");
}