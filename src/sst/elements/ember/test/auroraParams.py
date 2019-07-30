
debug = 0

netConfig = {
}

networkParams = {
    "packetSize" : "2048B",
    "link_bw" : "4GB/s",
    "xbar_bw" : "4GB/s",
    "link_lat" : "40ns",
    "input_latency" : "50ns",
    "output_latency" : "50ns",
    "flitSize" : "8B",
    "input_buf_size" : "14KB",
    "output_buf_size" : "14KB",
}

nicParams = {
    "nicComponent": 'aurora.nic',
    "nicSubComponent" : 'aurora.rdmaNic',
    "nicSubComponent.verboseLevel" : 0,
    "nicSubComponent.verboseMask" : -1,
    "nicSubComponent.clock" : '100Mhz',
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
    "os.name"      : "Host",
    "api.0.module" : "aurora.mpiLib",

    'aurora.mpiLib.verboseLevel' : 0,
    'aurora.mpiLib.verboseMask' : -1,
    'aurora.mpiLib.pt2pt.module' : 'aurora.rdmaMpiPt2PtLib',
    'aurora.mpiLib.pt2pt.numRecvBuffers' : 32,
    'aurora.mpiLib.pt2pt.verboseLevel' : 0,
    'aurora.mpiLib.pt2pt.verboseMask' : -1,

    'aurora.mpiLib.pt2pt.print_all_params'  : False,
    'aurora.mpiLib.pt2pt.rdmaLib.verboseLevel'  : 0,
    'aurora.mpiLib.pt2pt.rdmaLib.verboseMask'  : -1,

    "verbose" : 10,
    "verboseMask" : -1,
}

hermesParams = {
}
