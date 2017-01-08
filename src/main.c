/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.c                                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: alelievr <alelievr@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created  2016/12/23 17:42:25 by alelievr          #+#    #+#             */
/*   Updated  2016/12/23 17:42:25 by alelievr         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "printf_unit_test.h"
#include <fcntl.h>
#include <stdarg.h>

char *				g_current_format;
static int			g_current_index = 0;
static int			g_failed_tests = 0;
static int			g_passed_tests = 0;
static long long	g_current_arg;
static long			last_time_update;

static void	usage() __attribute__((noreturn));
static void	usage()
{
	printf("usage: ./run_test < converters >\n");
	printf("supported converters: \""SUPPORTED_CONVERTERS"\"");
	exit(-1);
}

static void	cout(const char *f, ...)
{
	va_list		ap;
	static int	new_stdout = 0;

	if (new_stdout == 0)
		new_stdout = open("/dev/tty", O_RDWR);

	va_start(ap, f);
	vdprintf(new_stdout, f, ap);
	va_end(ap);
}

static void	sigh(int s)
{
	cout(C_CRASH"catched signal: %s when testing format: \"%s\" with arg: \n", strsignal(s), g_current_index, g_current_format, g_current_arg);
	(void)s;
}

static void run_test(void (*testf)(char *b, int (*)(), int *, long long, int), int (*ft_printf)(), int fd[2], long long arg)
{
	char		buff[0xF00];
	char		printf_buff[0xF00];
	char		ftprintf_buff[0xF00];
	long		r;
	int			d1, d2; //d1 is the printf return and d2 ft_printf return.
	clock_t		b, m, e;

	g_current_arg = arg;
	b = clock();
	testf(g_current_format, ft_printf, &d1, arg, 0);
	m = clock();
	last_time_update = time(NULL); //for timeout
	testf(g_current_format, ft_printf, &d2, arg, 1);
	e = clock();
	fflush(stdout);
	if (d1 != d2)
	{
		cout(C_ERROR"bad return value for format: \"%s\": %i vs %i"C_CLEAR, g_current_format, d1, d2);
		g_failed_tests++;
	}
	r = read(fd[READ], buff, sizeof(buff));
	if (r > 0)
	{
		buff[r] = 0;
		if (!memchr(buff, '\x99', (size_t)r))
		{
			printf(C_ERROR"error while getting result on test: %s\n"C_CLEAR, g_current_format);
			return ;
		}
		size_t off = (size_t)((char *)memchr(buff, '\x99', (size_t)r) - buff);
		memcpy(printf_buff, buff, off);
		printf_buff[off] = 0;
		memcpy(ftprintf_buff, buff + off + 1, r + 1);
		if (strcmp(printf_buff, ftprintf_buff))
		{
			cout(C_ERROR"[ERROR] diff on output for format \"%s\" and arg: %li -> got: [%s], expected: [%s]\n"C_CLEAR, g_current_format, arg, ftprintf_buff, printf_buff);
			g_failed_tests++;
		}
		else
			g_passed_tests++;
		//split and diff the results
	}
	(void)index;
}

static void	run_tests(void *tests_h, int (*ft_printf)(), char *convs)
{
	char		fun_name[0xF00];
	int			fd[2];
	void		(*test)();
	int			index;
	int			total_test_count = 0;
	int			test_count = 0;
	int			old_failed_tests;
	long long	args[0xF0];
	int			argc;

	if (*convs)
		printf("Starting tests ...\n");
	if (pipe(fd) != 0)
		perror("pipe"), exit(-1);
	dup2(fd[WRITE], STDOUT_FILENO);
	close(fd[WRITE]);
	while (*convs)
	{
		old_failed_tests = g_failed_tests;
		cout(C_TITLE"testing %%%c ...\n"C_CLEAR, *convs);
		index = -1;
		test_count = 0;
		while (42)
		{
			index++;
			g_current_index = index;
			sprintf(fun_name, "printf_unit_test_%c_%.9i", *convs, index);
			if (!(test = (void(*)())dlsym(tests_h, fun_name)))
				break ;
			argc = generate_rand_args(*convs, args);
			for (int i = 0; i < argc; i++)
			{
				run_test(test, ft_printf, fd, args[i]);
				total_test_count++;
				test_count++;
			}
		}
		if (g_failed_tests == old_failed_tests)
			cout(C_PASS"Passed all %'i tests for convertion %c\n"C_CLEAR, test_count, *convs);
		else
			cout(C_ERROR"Failed %i tests for convertion %c\n"C_CLEAR, g_failed_tests - old_failed_tests, *convs);
		convs++;
	}
	cout("total format tested: %i\n", total_test_count);
}

static void	ask_download_tests(void)
{
	char	c;

	printf("main test library was not found, do you want to download it ? (y/n)");
	if ((c = (char)getchar()) == 'y' || c == 'Y' || c == '\n')
		system("curl -o printf-tests.so https://raw.githubusercontent.com/alelievr/printf-unit-test-libs/master/printf-tests.so?token=AGjy4wBye2wxE6bLiGPFtzg4B5W0wT-Bks5Ye7RLwA%3D%3D");
	else
		exit(0);
}

static void	*timeout_thread(void *t)
{
	(void)t;
	while (42)
	{
		sleep(1);
		if (time(NULL) - last_time_update > 3) //3sec passed on ftprintf function
		{
			cout(C_ERROR"Timeout on format: \"%s\" with argument: %lli\n", g_current_format, g_current_arg);
			exit(0);
		}
	}
}

int			main(int ac, char **av)
{
	static char		buff[0xF00];
	void			*tests_handler;
	void			*ftprintf_handler;
	char			*testflags = SUPPORTED_CONVERTERS;
	int				(*ft_printf)();
	pthread_t		p;

	if (ac > 2)
		usage();
	if (ac == 2)
		testflags = av[1];
	signal(SIGSEGV, sigh);
	signal(SIGBUS, sigh);
	signal(SIGPIPE, sigh);
	if (access(TEST_LIB_SO, F_OK))
		ask_download_tests();
	if (!(tests_handler = dlopen(TEST_LIB_SO, RTLD_LAZY)))
		perror("dlopen"), exit(-1);
	if (!(ftprintf_handler = dlopen(FTPRINTF_LIB_SO, RTLD_LAZY)))
		perror("dlopen"), exit(-1);
	if (!(ft_printf = (int (*)())dlsym(ftprintf_handler, "ft_printf")))
		perror("dlsym"), exit(-1);
	g_current_format = buff;
	if ((pthread_create(&p, NULL, timeout_thread, NULL)) == -1)
		puts(C_ERROR"thread init failed"), exit(-1);
	run_tests(tests_handler, ft_printf, testflags);
	return (0);
}
