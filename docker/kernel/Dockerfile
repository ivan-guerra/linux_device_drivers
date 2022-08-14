# Create a docker image that contains the tools needed to build the Linux
# kernel.

FROM kbase:latest

# It's required that when calling docker build, all the info needed to create
# a clone of the host user is provided. This simplifies things like re-using
# object files between docker runs to speed up builds and keeping paths
# consistent so that tools like GDB can resolve source files.
ARG USER
ARG HOME
ARG USER_ID
ARG GROUP_ID

# Add the user group and user account.
RUN groupadd -g ${GROUP_ID} $USER &&\
    useradd -l -u ${USER_ID} -g $USER $USER &&\
    install -d -m 0755 -o $USER -g $USER $HOME

# Install the kernel build script in the user's home directory.
COPY kbuild.sh $HOME
RUN chown $USER:$USER $HOME/kbuild.sh &&\
    chmod +x $HOME/kbuild.sh

USER $USER
WORKDIR $HOME

CMD ["/bin/bash", "-c", "./kbuild.sh"]
