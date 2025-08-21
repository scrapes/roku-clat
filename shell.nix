{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  buildInputs = with pkgs; [
    # C compiler and build tools
    gcc
    gnumake
    
    # Development headers for networking and system calls
    glibc.dev
    
    # Static libraries for static linking
    glibc.static
  ];
  
  # Set environment variables for the build
  shellHook = ''
    export CC=gcc
    export CFLAGS="-std=gnu11 -Wall -Werror -O2 -g"
    export LDFLAGS="-static"
    export LIBS="-static"
  '';
}
