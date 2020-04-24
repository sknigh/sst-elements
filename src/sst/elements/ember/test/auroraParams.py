
debug = 0

netConfig = {
}

networkParams = {
    "packetSize" : "2048B",
    "link_bw" : "8GB/s",
    "xbar_bw" : "8GB/s",

    "link_lat" : "40ns",
    "input_latency" : "50ns",
    "output_latency" : "50ns",
    "flitSize" : "8B",
    "input_buf_size" : "14KB",
    "output_buf_size" : "14KB",
}

nicParams = {
    "nicComponent": 'aurora.nic',
    "nicSubComponent.verboseLevel" : 0,
    "nicSubComponent.verboseMask" : 1<<1 ,
    "nicSubComponent.verboseMask" : -1,
    "nicSubComponent.clock" : '500Mhz',
    "nicSubComponent.toHostLatency" : 150,
    "nicSubComponent.rxLatency" : 0,
    "nicSubComponent.txLatency" : 0,
    "nicSubComponent.fromHostBandwidth" : "15GB/s",
    "nicSubComponent.toHostBandwidth"   : "15GB/s",
    "nicSubComponent.maxBuffers" : 64,
    "verboseLevel" : 0,
    "verboseMask" : -1,
    "module" : "merlin.reorderlinkcontrol",
    #"module" : "merlin.linkcontrol",
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
    'host.toNicLatency' : 150,
    "host.maxNicQdepth" : 64,

    'aurora.mpiLib.verboseLevel' : 0,
    'aurora.mpiLib.verboseMask' : -1,

    'aurora.mpiLib.pt2pt.verboseLevel' : 0,
    'aurora.mpiLib.pt2pt.verboseMask' : -1,
    #'aurora.mpiLib.pt2pt.shortMsgLength' : 1000000,
    'aurora.mpiLib.pt2pt.shortMsgLength' : 16000,
    'aurora.mpiLib.pt2pt.print_all_params'  : False,
    'aurora.mpiLib.pt2pt.defaultLatency' : 0,
    'aurora.mpiLib.pt2pt.matchLatency' : 0,
    'aurora.mpiLib.pt2pt.memcpyBandwidth' : "10GB/s",
    'aurora.mpiLib.pt2pt.numRecvBuffers' : 64,
    'aurora.mpiLib.pt2pt.numSendBuffers' : 32,

    "verbose" : 0,
    "verboseMask" : -1,
}

hermesParams = {
}
