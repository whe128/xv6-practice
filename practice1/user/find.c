#include "types.h"
#include "stat.h"
#include "fs.h"
#include "user.h"

void find(char *path, char *target)
{
	char buf[512];
	char *p;
	int fd;
	struct stat st;
	struct dirent de;
	fd = open(path, 0); // open the file

	// open error
	if (fd < 0)
	{
		printf(2, "find: cannot open %s\n", path);
		return;
	}

	if (fstat(fd, &st) < 0)
	{
		printf(2, "find: cannot stat %s\n", path);
		close(fd);
		return;
	}

	// now we get the information

	p = path + strlen(path);

	// find the last name/
	while (p >= path && *p != '/')
	{
		p--; // from end to start
	}
	p++;

	// successfully find
	if (strcmp(p, target) == 0)
	{
		printf(1, "%s\n", path);
	}

	// if it is directory, we continue to find deeply
	if (st.type == T_DIR)
	{
		// we need a new copy for base path
		strcpy(buf, path);
		p = buf + strlen(buf);
		*p = '/';
		p++;
		*p = '\0';

		// directory store multiple dirent
		while (read(fd, &de, sizeof(de)) == sizeof(de))
		{
			// each item

			// a empty dir
			if (de.inum == 0)
			{
				continue;
			}

			// current dir or previous dir
			if (strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)
			{
				continue;
			}

			// not empty, explore further
			strcpy(p, de.name);

			find(buf, target);
		}
	}
	close(fd);
}

int main(int argc, void *argv[])
{
	if (argc != 3)
	{
		printf(2, "Usage: find <path> <filename>");
		exit();
	}
	// argv[1] is path, argv[2] is target name
	find(argv[1], argv[2]);
	exit();
}
