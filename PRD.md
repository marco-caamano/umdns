# Micro mDNS tools (umDNS)

umdns are a group of tools to query, browse and respond to mDNS queries on the local network.

umdns provide a simple and small implementation of mDNS, but do not implement all the features, just enough to meet the listed features

It is composed by:
- Client: called `umdns_client` that will accept either a hostname or a service and then bind to a network interface and perform the mDNS query, then wait up to timeout seconds for any response, then print the results.
- Server: called `umdns_server` that will bind to a given interface and then listen for any mDNS queries and respond with the local hostname or the local registered services
- Browser: called `umdns_browser` that will bind to a network interface and browse the network for any hosts or services via mDNS (it may send queries to all hosts on the network)

## Technology
- Use a C99 implemnentation with strict flags
- Builds return no warnings

## General Features
- Supports both IPv6 and IPv4
- Uses timeouts when waiting for replies
- Share common code under a common directory
- all utilites support logging to console or file with different logging levels, default to INFO
- Provides command help details with examples

## Server Features
- Suport hostname discovery, service discovery
- Server can parse a config file in INI-style that will register services
- Supports adding custom TXT fields to both the hostname resolution as the service resolution
- Gracefull shutdown via SIGINT/SIGTERM

## Client Features
- Connects to a specific network interface
- accepts the timeout as a parameter
- defaults to perform a nDNS hostname query
- supports Service Discovery (SRV/TXT records)
- Reports results in a structured table with enough details
- Only tries to perform a hostname or service resolution

## Browser Features
- Will send mDNS queries to all nodes to determine all available host and services on the .local domain
- Will accept a timeout parameter for how long to wait before exiting. 
- If timeout is zero it will continue to loop sending queries and waiting for responses until interrupted
- Gracefull shutdown via SIGINT/SIGTERM
- Accepts a parameter to configure the interval between sending mDNS queries to all hosts in the network

## Documentation
Create README.md documentation
- Add top table of contents with links to all sections and subsections
- At the bottom of each section add a link to return to the top
- Add design and architecture diagrams and documentation
- Provide detailed examples
- Documents core shared modules as well as specific tool modules

## Installation
- Provide a local install Makefile target (~/.local/bin/)
- Provide a remote install Makefile target that will scp the binaries to a target host 