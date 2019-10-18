
debug = 0

netConfig = {
}

networkParams = {
    #"packetSize" : "2048B",
    "packetSize" : "10000B",
    "link_bw" : "8GB/s",
    "xbar_bw" : "8GB/s",
    #"link_bw" : "1000GB/s",
    #"xbar_bw" : "1000GB/s",
    "link_lat" : "1ns",
    "input_latency" : "1ns",
    "output_latency" : "1ns",
    #"link_lat" : "40ns",
    #"input_latency" : "50ns",
    #"output_latency" : "50ns",
    "flitSize" : "8B",
    "input_buf_size" : "14KB",
    "output_buf_size" : "14KB",
}

nicParams = {
    "nicComponent": 'aurora.nic',
    "nicSubComponent.verboseLevel" : 0,
    "nicSubComponent.verboseMask" : -1,
    "nicSubComponent.clock" : '5000Mhz',
    "nicSubComponent.toHostLatency" : 0,
    "nicSubComponent.rxLatency" : 10,
    "nicSubComponent.txLatency" : 10,
    "nicSubComponent.fromHostBandwidth" : "2GB/s",
    "nicSubComponent.toHostBandwidth"   : "1GB/s",
    "verboseLevel" : 0,
    "verboseMask" : -1,
    "module" : "merlin.linkcontrol",
    "packetSize" : networkParams['packetSize'],
    "link_bw" : networkParams['link_bw'],
    "input_buf_size" : networkParams['input_buf_size'],
    "output_buf_size" : networkParams['output_buf_size'],
}

emberParams = {
    "os.module"    : "aurora.host",
    "os.name"      : "host",
    "api.0.module" : "aurora.mpiLib",

    'host.print_all_params' : 0,
    'host.verboseLevel' : 0,
    'host.verboseMask' : -1,
    'host.toNicLatency' : 0,
    "host.maxNicQdepth" : 64,

    'aurora.mpiLib.verboseLevel' : 0,
    'aurora.mpiLib.verboseMask' : -1,

    'aurora.mpiLib.pt2pt.verboseLevel' : 0,
    'aurora.mpiLib.pt2pt.verboseMask' : -1,
    'aurora.mpiLib.pt2pt.print_all_params'  : False,
    'aurora.mpiLib.pt2pt.defaultLatency' : 0,
    'aurora.mpiLib.pt2pt.matchLatency' : 0,
    #'aurora.mpiLib.pt2pt.memcpyBandwidth' : "1GB/s",
    'aurora.mpiLib.pt2pt.numRecvBuffers' : 32,

    "verbose" : 0,
    "verboseMask" : -1,
}

hermesParams = {
}
