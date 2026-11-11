SRCS= $(addprefix srcs/, \
    main.c \
    cli/init_options.c \
    cli/cli_options.c \
    cli/cli_options_parser.c \
    elf/elf_header.c \
    elf/elf_sections.c \
    elf/elf_symbols.c \
    elf/symbol_types.c \
    utils/error.c \
    utils/file_utils.c \
    utils/read_utils.c \
    utils/sort_symbols.c \
    utils/output.c \
    archive/archive_process.c \
    )
OBJS= $(addprefix .build/mandatory/, $(SRCS:.c=.o))
BONUS_OBJS= $(addprefix .build/bonus/, $(SRCS:.c=.o))

NAME= ft_nm
CC= gcc
FLAGS= -Wall -Werror -Wextra
CFLAGS= $(FLAGS)
INCLUDES= -Iincludes/

all: $(NAME)

$(NAME): .build/mandatory/$(NAME) FORCE
	@if ! cmp -s $< $(NAME); then cp $< $(NAME); fi

.build/mandatory/$(NAME): $(OBJS)
	@$(CC) $(CFLAGS) $(OBJS) $(LDFLAGS) -o $@

bonus: .build/bonus/$(NAME)
	@if ! cmp -s $< $(NAME); then cp $< $(NAME); fi

.build/bonus/$(NAME): $(BONUS_OBJS)
	@$(CC) $(CFLAGS) $(BONUS_OBJS) $(LDFLAGS) -o $@

.build/mandatory/%.o: %.c includes/ft_nm.h Makefile
	@mkdir -p $(@D)
	@$(CC) $(CFLAGS) $(INCLUDES) -MMD -MP -c $< -o $@

.build/bonus/%.o: %.c includes/ft_nm.h Makefile
	@mkdir -p $(@D)
	@$(CC) $(CFLAGS) $(INCLUDES) -DBONUS -MMD -MP -c $< -o $@

clean:
	@rm -rf .build

fclean: clean
	@rm -f $(NAME)

re: fclean
	@$(MAKE) --no-print-directory all

-include $(OBJS:.o=.d) $(BONUS_OBJS:.o=.d)

.PHONY: all bonus clean fclean re
