# Create a docker image that contains the tools needed to build the Linux
# kernel.

FROM debian:10.8-slim

# Install the minimum set of tools needed to build the Linux kernel.
RUN apt-get update     &&\
    apt-get install -y   \
        bc               \
        bison            \
        build-essential  \
        cpio             \
        flex             \
        libelf-dev       \
        libncurses-dev   \
        libssl-dev       \
        vim-tiny         \
        curl

# When building the container the User has the option to specify a UID/GID
# to be assigned to the image's default user. This comes in handy when sharing
# data between host and container via volumes. See
# container via volumes. See https://jtreminio.com/blog/running-docker-containers-as-current-host-user/#ok-so-what-actually-works
ARG USER_ID=1000
ARG GROUP_ID=1000

ENV USER="kdev"
ENV USER_HOME="/home/$USER"
ENV HOME="/home/kdev/"

# Add the user group and user account.
RUN groupadd -g ${GROUP_ID} $USER &&\
    useradd -l -u ${USER_ID} -g $USER $USER &&\
    install -d -m 0755 -o $USER -g $USER $USER_HOME

# Install the kernel build script in the user's home directory.
COPY kbuild.sh $USER_HOME
RUN chown $USER:$USER $USER_HOME/kbuild.sh &&\
    chmod +x $USER_HOME/kbuild.sh

USER $USER
WORKDIR $USER_HOME

CMD ["/bin/bash", "-c", "./kbuild.sh"]