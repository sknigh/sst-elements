import os,sys
import sst
from mhlib import componentlist

quiet = True

memCapacity = 4 # In GB
memPageSize = 4 # in KB
memNumPages = memCapacity * 1024 * 1024 / memPageSize

mesh_stops_x        = 3
mesh_stops_y        = 3

mesh_clock          = 2200
ctrl_mesh_flit      = 8
data_mesh_flit      = 36
mesh_link_latency   = "100ps"    # Note, used to be 50ps, didn't seem to make a difference when bumping it up to 100
ctrl_mesh_link_bw   = str( (mesh_clock * 1000 * 1000 * ctrl_mesh_flit) ) + "B/s"
data_mesh_link_bw   = str( (mesh_clock * 1000 * 1000 * data_mesh_flit) ) + "B/s"

core_clock         = "1800MHz"
coherence_protocol = "MESI"

ctrl_network_buffers = "32B"
data_network_buffers = "288B"

ctrl_network_params = {
        "link_bw" : ctrl_mesh_link_bw,
        "flit_size" : str(ctrl_mesh_flit) + "B",
        "input_buf_size" : ctrl_network_buffers,
}

data_network_params = {
        "link_bw" : data_mesh_link_bw,
        "flit_size" : str(data_mesh_flit) + "B",
        "input_buf_size" : data_network_buffers,
        "port_priority_equal" : 1,
}

# Debug parameters for memH
debugAll = 0
debugL1 = max(debugAll, 0)
debugL2 = max(debugAll, 0)
debugDDRDC = max(debugAll, 0)
debugMemCtrl = max(debugAll, 0)
debugNIC = max(debugAll, 0)
debugLev = 0 

# Verbose
verbose = 0 

#cpuName = "memHierarchy.spmvCpu"
cpuName = "memHierarchy.gupsCpu"

cpu_params = {
    "verbose" : 0,
    "clock" : core_clock,
    "printStats" : 1,
    "iterations": 100,
    "debug": 1,
    "debug_level": 0,
    #"debug_mask": 1 << 0 | 1<< 2 ,
    #"debug_mask": 1<<2,
    "activeThreadsPerNode" : 36,
}

NumComputeNodes = 1
NumFamNodes = 1 
NumComputeThreads = 1 #36*NumComputeNodes

SpmvMatrixSize = 36 # 144*1

def calcMyComputeThread( nodeId, threadsPerNode, coreNum ):
    return nodeId * threadsPerNode + coreNum

def configureGups( params, myThread ):    
	#params['activeThreadsPerNode'] = 1
	params[ "gupsMemSize" ] = 0x100000
	return params

def configureSpmv( params, myThread ):    
    params[ "matrix_nx"] = SpmvMatrixSize 
    params[ "matrix_ny"] = SpmvMatrixSize 

    localRowStart = SpmvMatrixSize / NumComputeThreads * myThread
    params[ "local_row_start" ] = localRowStart 
    params[ "local_row_end" ] = localRowStart + SpmvMatrixSize  / NumComputeThreads
    params[ "firstFamNode" ] = NumComputeNodes 
    params[ "numFamNodes" ] = NumFamNodes 

    if myThread < NumComputeThreads:
        params[ "computeNode"] = "yes" 
    else:
        params[ "computeNode"] = "no" 

    return params


def createCpuParams( nicId, threadsPerNode, coreNum ):
    #print('nicId={} numThreads={} core={}').format( nicId, threadsPerNode, coreNum )
    params = cpu_params
    params[ "nodeNum"] =  nicId 
    params[ "threadNum"] = coreNum

    # for SHMEM
    params[ "pe"] =  nicId * threadsPerNode + coreNum

    myThread = calcMyComputeThread( nicId, threadsPerNode, coreNum )
    #params = configureSpmv( params, myThread )
    params = configureGups( params, myThread )
    
    return params

nic_params = {
    "clock" : "8GHz",
    "debug":1,
    "debug_level": 0,
    #"debug_mask": 1<<1 | 1 << 2,
    #"debug_mask": 1<<1,
    "debug_mask": -1,
    "maxPendingCmds" : 128,
    "maxMemReqs" : 256,
    "maxCmdQSize" : 128,
    "cache_line_size"    : 64,
}

l1_cache_params = {
    "cache_frequency"    : core_clock,
    "coherence_protocol" : coherence_protocol,
    "replacement_policy" : "lru",
    "cache_size"         : "32KiB",
    "associativity"      : 8,
    "cache_line_size"    : 64,
    "access_latency_cycles" : 4,
    "tag_access_latency_cycles" : 1,
    "mshr_num_entries"   : 12, # Outstanding misses per core
    "maxRequestDelay"    : 10000000,
    "events_up_per_cycle" : 2,
    "mshr_latency_cycles" : 2,
    "max_requests_per_cycle" : 1,
    #"request_link_width" : "72B",
    #"response_link_width" : "36B",
    "L1"                 : 1,
    "verbose"            : verbose,
    "debug"              : debugL1,
    "debug_level"        : debugLev,
}

#l2_prefetch_params = {
#    "prefetcher" : "cassini.StridePrefetcher",
#    "prefetcher.reach" : 16,
#    "prefetcher.detect_range" : 1
#}
l2_prefetch_params = {
    "reach" : 16,
    "detect_range" : 1
}

l2_cache_params = {
    "cache_frequency"    : core_clock,
    "coherence_protocol" : coherence_protocol,
    "replacement_policy" : "lru",
    "cache_size"         : "1MiB",
    "associativity"      : 16,
    "cache_line_size"    : 64,
    "access_latency_cycles" : 8,   # Guess - co-processor s/w dev guide says 11 for 512KiB cache
    "tag_access_latency_cycles" : 3,
    "mshr_num_entries"   : 48, # Actually 48 reads and 32 writebacks
    #"max_requests_per_cycle" : 2,
    "mshr_latency_cycles" : 4,
    #"request_link_width" : "72B",
    "response_link_width" : "72B",
    "verbose" : verbose,
    "debug"              : debugL2,
    "debug_level"        : debugLev
}

l2_nic_params = {
    "group" : 1,
    "destinations" : "2,5",
    "debug" : debugNIC,
    "debug_level" : debugLev,
}

ctrl_net_params = {
    "link_bw" : ctrl_mesh_link_bw,
    "in_buf_size" : ctrl_network_buffers,
    "out_buf_size" : ctrl_network_buffers
}

data_net_params = {
    "link_bw" : data_mesh_link_bw,
    "in_buf_size" : data_network_buffers,
    "out_buf_size" : data_network_buffers
}

###### DDR Directory #######
ddr_dc_params = {
    "coherence_protocol": coherence_protocol,
    "clock"             : str(mesh_clock) + "MHz",
    "entry_cache_size"  : 256*1024*1024, #Entry cache size of mem/blocksize
    "mshr_num_entries"  : 128,
    "access_latency_cycles" : 2,
    "verbose" : verbose,
    "debug"             : debugDDRDC,
    "debug_level"       : debugLev
}

dc_nic_params = {
    "group" : 2,
    "sources": "1,5",
    "destinations" : "3",
}

##### TimingDRAM #####
# DDR4-2400
ddr_mem_timing_params = {
    "verbose" : verbose,
    #"backing" : "none",
    "backing" : "mmap",
    "clock"   : "1200MHz",
    #"clock"   : "2400MHz",
}

ddr_backend_params = {
    "id" : 0,
    "addrMapper" : "memHierarchy.simpleAddrMapper",
    "channel.transaction_Q_size" : 32,
    "channel.numRanks" : 2,
    "channel.rank.numBanks" : 16,
    "channel.rank.bank.CL" : 15,
    "channel.rank.bank.CL_WR" : 12,
    "channel.rank.bank.RCD" : 15,
    "channel.rank.bank.TRP" : 15,
    "channel.rank.bank.dataCycles" : 4,
    "channel.rank.bank.pagePolicy" : "memHierarchy.simplePagePolicy",
    "channel.rank.bank.transactionQ" : "memHierarchy.reorderTransactionQ",
    "channel.rank.bank.pagePolicy.close" : 0,
    "printconfig" : 0,
    "channel.printconfig" : 0,
    "channel.rank.printconfig" : 0,
    "channel.rank.bank.printconfig" : 0,
}

ddr_nic_params = {
    "group" : 3,
    "accept_region" : 0,
}

shmemNic_nic_params = {
    "group" : 5,
    "sources" : "1",
    "destinations" : "2",
}

# Miranda STREAM Bench params        
thread_iters = 1000


gen_params = {
    "verbose" : 0,
    "n" : thread_iters,
    "operandWidth" : 8,
}

class DDRBuilder:
    def __init__(self, capacity):
        self.init();
        self.mem_capacity = capacity

    def init(self):
        self.next_ddr_id = 0

    def build(self, nodeID):
        if not quiet:
            print "Creating DDR controller " + str(self.next_ddr_id) + " out of 4 on node " + str(nodeID) + "..."
            print " - Capacity: " + str(self.mem_capacity / 4) + " per DDR."

        mem = sst.Component("ddr_" + str(self.next_ddr_id), "memHierarchy.MemController")
        mem.addParams(ddr_mem_timing_params)
        
        membk = mem.setSubComponent("backend", "memHierarchy.timingDRAM")
        membk.addParams({ "mem_size" : str(self.mem_capacity / 4) + "B" })
        membk.addParams(ddr_backend_params)
        
        memNIC = mem.setSubComponent("cpulink", "memHierarchy.MemNICFour")
        memNIC.addParams(ddr_nic_params)
        memdata = memNIC.setSubComponent("data", "kingsley.linkcontrol")
        memreq = memNIC.setSubComponent("req", "kingsley.linkcontrol")
        memack = memNIC.setSubComponent("ack", "kingsley.linkcontrol")
        memfwd = memNIC.setSubComponent("fwd", "kingsley.linkcontrol")
        memdata.addParams(data_net_params)
        memreq.addParams(ctrl_net_params)
        memfwd.addParams(ctrl_net_params)
        memack.addParams(ctrl_net_params)

        #print 'DDR', (64 * self.next_ddr_id), (self.mem_capacity - (64 * self.next_ddr_id))
        mem.addParams({
            "addr_range_start" : (64 * self.next_ddr_id),
            "addr_range_end" : (self.mem_capacity - (64 * self.next_ddr_id)),
            "interleave_step" : str(4 * 64) + "B",
            "interleave_size" : "64B",
        })
        self.next_ddr_id = self.next_ddr_id + 1
        return (memreq, "rtr_port", mesh_link_latency), (memack, "rtr_port", mesh_link_latency), (memfwd, "rtr_port", mesh_link_latency), (memdata, "rtr_port", mesh_link_latency)


class DDRDCBuilder:
    def __init__(self, capacity):
        self.init(0)
        self.memCapacity = capacity


    def init(self,nicID):
        self.next_ddr_dc_id = 0
        self.nicID = nicID

    def build(self, nodeID):
        # Stripe addresses across each mem & stripe those across each DC for the mem
        #   Interleave 64B blocks across 8 DCs (and then map every 4th to the same DDR)
       
        dcNum = nodeID % 2
        if nodeID == 1 or nodeID == 2:
            memId = 0
        elif nodeID == 3 or nodeID == 6:
            memId = 1
        elif nodeID == 4 or nodeID == 7:
            memId = 2
        elif nodeID == 5 or nodeID == 8:
            memId = 3
        
        myStart = 0 + (memId * 64) + (dcNum * 64 * 4)
        myEnd = self.memCapacity - 64 * (8 - memId - 4 * dcNum) + 63

        if not quiet:
            print "\tCreating ddr dc with start: " + str(myStart) + " end: " + str(myEnd)

        dc = sst.Component("ddr_dc_" + str(self.next_ddr_dc_id), "memHierarchy.DirectoryController")
        dc.addParams(ddr_dc_params)

        dc.addParams({
            "addr_range_start" : myStart,
            "addr_range_end" : myEnd,
            "interleave_step" : str(8 * 64) + "B",
            "interleave_size" : "64B",
            "net_memory_name" : "nodeId" + str(self.nicID) + ".ddr_" + str(memId),
        })
        #print '{} {} {} {} {}'.format( "ddr_dc_" + str(self.next_ddr_dc_id),myStart, myEnd, str(8 * 64) + "B", "64B") 
        # Create NIC on to interface to NoC from directory
        dcNIC = dc.setSubComponent("cpulink", "memHierarchy.MemNICFour")
        dcNIC.addParams(dc_nic_params)
        dcdata = dcNIC.setSubComponent("data", "kingsley.linkcontrol")
        dcreq = dcNIC.setSubComponent("req", "kingsley.linkcontrol")
        dcfwd = dcNIC.setSubComponent("fwd", "kingsley.linkcontrol")
        dcack = dcNIC.setSubComponent("ack", "kingsley.linkcontrol")
        dcreq.addParams(ctrl_net_params)
        dcfwd.addParams(ctrl_net_params)
        dcack.addParams(ctrl_net_params)
        dcdata.addParams(data_net_params)

        self.next_ddr_dc_id = self.next_ddr_dc_id + 1
        return (dcreq, "rtr_port", mesh_link_latency), (dcack, "rtr_port", mesh_link_latency), (dcfwd, "rtr_port", mesh_link_latency), (dcdata, "rtr_port", mesh_link_latency)


class TileBuilder:
    def __init__(self):
        self.init(0,0)
        self.pesPerNode = 36

    def init(self,nicId,nicBaseAddr):
        self.nicId = nicId
        self.nicBaseAddr = nicBaseAddr 
        self.next_tile_id = 0
        self.next_core_id = 0
        self.next_addr_id = 0
        self.base_a = 0
        self.base_b = thread_iters * 8 * 36
        self.base_c = self.base_b + thread_iters * 8 * 36

    def build(self, nodeID):
        #print('build tile nodeID={0} next_tile_id={1} {2} {3}').format(nodeID, self.next_tile_id, self.next_core_id, self.next_addr_id )
        # L2
        tileL2cache = sst.Component("l2cache_" + str(self.next_tile_id), "memHierarchy.Cache")
        tileL2cache.addParams(l2_cache_params)
        # l2 prefetcher
        l2pre = tileL2cache.setSubComponent("prefetcher", "cassini.StridePrefetcher")
        l2pre.addParams(l2_prefetch_params)
        # l2 bus link
        l2tol1 = tileL2cache.setSubComponent("cpulink", "memHierarchy.MemLink")
        # l2 NIC
        l2NIC = tileL2cache.setSubComponent("memlink", "memHierarchy.MemNICFour")
        l2data = l2NIC.setSubComponent("data", "kingsley.linkcontrol")
        l2req = l2NIC.setSubComponent("req", "kingsley.linkcontrol")
        l2fwd = l2NIC.setSubComponent("fwd", "kingsley.linkcontrol")
        l2ack = l2NIC.setSubComponent("ack", "kingsley.linkcontrol")
        l2NIC.addParams(l2_nic_params)
        l2data.addParams(data_net_params)
        l2req.addParams(ctrl_net_params)
        l2fwd.addParams(ctrl_net_params)
        l2ack.addParams(ctrl_net_params)

        # Bus (from l1s to l2)
        l2bus = sst.Component("l2cachebus_" + str(self.next_tile_id), "memHierarchy.Bus")
        l2bus.addParams({ "bus_frequency" : core_clock })

        l2busLink = sst.Link("l2bus_link_" + str(self.next_tile_id))
        l2busLink.connect( (l2bus, "low_network_0", mesh_link_latency),
            (l2tol1, "port", mesh_link_latency))
        l2busLink.setNoCut()

        self.next_tile_id = self.next_tile_id + 1


        # Left Core L1
        tileLeftL1 = sst.Component("l1cache_" + str(self.next_core_id), "memHierarchy.Cache")
        tileLeftL1.addParams(l1_cache_params)

        if not quiet:
            print "Creating core " + str(self.next_core_id) + " on tile: " + str(self.next_tile_id) + "..."

        # Left SMT
        leftSMT = sst.Component("smt_" + str(self.next_core_id), "memHierarchy.multithreadL1")
        leftSMT.addParams({
            "clock" : core_clock,
            "requests_per_cycle" : 2,
            "responses_per_cycle" : 2,
            })

        # Left Core
        cpuL0 = sst.Component("thread_" + str(self.next_core_id), cpuName )
        cpuL1 = sst.Component("thread_" + str(self.next_core_id + 18), cpuName )
        cpuL0.addParams( createCpuParams( self.nicId, self.pesPerNode, self.next_core_id ) )
        cpuL1.addParams( createCpuParams( self.nicId, self.pesPerNode, self.next_core_id + 18 ) )

        # Thread 0
        leftSMTCPUlink0 = sst.Link("smt_cpu_" + str(self.next_core_id))
        leftSMTCPUlink0.connect( (cpuL0, "cache_link", mesh_link_latency), (leftSMT, "thread0", mesh_link_latency) )
        # Thread 1
        leftSMTCPUlink1 = sst.Link("smt_cpu_" + str(self.next_core_id + 18))
        leftSMTCPUlink1.connect( (cpuL1, "cache_link", mesh_link_latency), (leftSMT, "thread1", mesh_link_latency) )
        # SMT Shim <-> L1
        leftSMTL1link = sst.Link("l1cache_smt_" + str(self.next_core_id))
        leftSMTL1link.connect( (leftSMT, "cache", mesh_link_latency), (tileLeftL1, "high_network_0", mesh_link_latency) )

        leftSMTCPUlink0.setNoCut()
        leftSMTCPUlink1.setNoCut()
        leftSMTL1link.setNoCut()

        leftL1L2link = sst.Link("l1cache_link_" + str(self.next_core_id))
        leftL1L2link.connect( (l2bus, "high_network_0", mesh_link_latency),
            (tileLeftL1, "low_network_0", mesh_link_latency))
        leftL1L2link.setNoCut()

        self.next_core_id = self.next_core_id + 1

        tileRightL1 = sst.Component("l1cache_" + str(self.next_core_id), "memHierarchy.Cache")
        tileRightL1.addParams(l1_cache_params)

        if not quiet:
            print "Creating core " + str(self.next_core_id) + " on tile: " + str(self.next_tile_id) + "..."
        
        # Right SMT
        rightSMT = sst.Component("smt_" + str(self.next_core_id), "memHierarchy.multithreadL1")
        rightSMT.addParams({
            "clock" : core_clock,
            "requests_per_cycle" : 2,
            "responses_per_cycle" : 2,
            })

        # Right Core
        cpuR0 = sst.Component("thread_" + str(self.next_core_id), cpuName )
        cpuR1 = sst.Component("thread_" + str(self.next_core_id + 18), cpuName )
        cpuR0.addParams( createCpuParams( self.nicId, self.pesPerNode, self.next_core_id ) )
        cpuR1.addParams( createCpuParams( self.nicId, self.pesPerNode, self.next_core_id + 18 ) )

        # Thread 0
        rightSMTCPUlink0 = sst.Link("smt_cpu_" + str(self.next_core_id))
        rightSMTCPUlink0.connect( (cpuR0, "cache_link", mesh_link_latency), (rightSMT, "thread0", mesh_link_latency) )
        # Thread 1
        rightSMTCPUlink1 = sst.Link("smt_cpu_" + str(self.next_core_id + 18))
        rightSMTCPUlink1.connect( (cpuR1, "cache_link", mesh_link_latency), (rightSMT, "thread1", mesh_link_latency) )
        # SMT Shim <-> L1
        rightSMTL1link = sst.Link("l1cache_smt_" + str(self.next_core_id))
        rightSMTL1link.connect( (rightSMT, "cache", mesh_link_latency), (tileRightL1, "high_network_0", mesh_link_latency) )

        rightSMTCPUlink0.setNoCut()
        rightSMTCPUlink1.setNoCut()
        rightSMTL1link.setNoCut()

        rightL1L2link = sst.Link("l1cache_link_" + str(self.next_core_id))
        rightL1L2link.connect( (l2bus, "high_network_1", mesh_link_latency),
                        (tileRightL1, "low_network_0", mesh_link_latency))
        rightL1L2link.setNoCut()

        self.next_core_id = self.next_core_id + 1

        return (l2req, "rtr_port", mesh_link_latency), (l2ack, "rtr_port", mesh_link_latency), (l2fwd, "rtr_port", mesh_link_latency), (l2data, "rtr_port", mesh_link_latency)


tileBuilder = TileBuilder()
memBuilder  = DDRBuilder(memCapacity * 1024 * 1024 * 1024)
DCBuilder = DDRDCBuilder(memCapacity * 1024 * 1024 * 1024)

def setNodeDist(nodeId, rtrreq, rtrack, rtrfwd, rtrdata):
    port = nodeId % 2   # Even port = tile, odd = DC
    actNode = nodeId // 2

    if nodeId == 1 or nodeId == 3 or nodeId == 5 or nodeId == 7:
        req, ack, fwd, data = memBuilder.build(nodeId)
        if nodeId == 1:
            port = "north"
        elif nodeId == 3:
            port = "west"
        elif nodeId == 5:
            port = "east"
        elif nodeId == 7:
            port = "south"
        
        rtrreqport = sst.Link("krtr_req_" + port + "_" +str(nodeId))
        rtrreqport.connect( (rtrreq, port, mesh_link_latency), req )
        rtrackport = sst.Link("krtr_ack_" + port + "_" + str(nodeId))
        rtrackport.connect( (rtrack, port, mesh_link_latency), ack )
        rtrfwdport = sst.Link("krtr_fwd_" + port + "_" + str(nodeId))
        rtrfwdport.connect( (rtrfwd, port, mesh_link_latency), fwd )
        rtrdataport = sst.Link("kRtr_data_" + port + "_" + str(nodeId))
        rtrdataport.connect( (rtrdata, port, mesh_link_latency), data )

    # Place tiles on all routers
    tilereq, tileack, tilefwd, tiledata = tileBuilder.build(nodeId)
    reqport0 = sst.Link("krtr_req_port0_" + str(nodeId))
    reqport0.connect( (rtrreq, "local0", mesh_link_latency), tilereq )
    ackport0 = sst.Link("krtr_ack_port0_" + str(nodeId))
    ackport0.connect( (rtrack, "local0", mesh_link_latency), tileack )
    fwdport0 = sst.Link("krtr_fwd_port0_" + str(nodeId))
    fwdport0.connect( (rtrfwd, "local0", mesh_link_latency), tilefwd )
    dataport0 = sst.Link("kRtr_data_port0_" + str(nodeId))
    dataport0.connect( (rtrdata, "local0", mesh_link_latency), tiledata )

    # Place DC at every tile except 0
    if nodeId != 0:
        req, ack, fwd, data = DCBuilder.build(nodeId)
        reqport1 = sst.Link("krtr_req_port1_" + str(nodeId))
        reqport1.connect( (rtrreq, "local1", mesh_link_latency), req )
        ackport1 = sst.Link("krtr_ack_port1_" + str(nodeId))
        ackport1.connect( (rtrack, "local1", mesh_link_latency), ack )
        fwdport1 = sst.Link("krtr_fwd_port1_" + str(nodeId))
        fwdport1.connect( (rtrfwd, "local1", mesh_link_latency), fwd )
        dataport1 = sst.Link("kRtr_data_port1_" + str(nodeId))
        dataport1.connect( (rtrdata, "local1", mesh_link_latency), data )

class NicBuilder:
    def __init__(self ):
        self.cmdQ = 1 

    def build(self, nodeID, numNodes, pesPerNode, nicBaseAddr, cmdQsize, link_bw, input_buf_size, output_buf_size ):

        nic = sst.Component( "nic", "memHierarchy.ShmemNic")

        print('nodeId={} numNodes={} pesPerNode={} nicBaseAddr={} cmdQsize={}').format( nodeID, numNodes, pesPerNode, nicBaseAddr, cmdQsize )
        nic.addParams( nic_params )
        nic.addParam( 'nicId', nodeID )
        nic.addParam( 'cmdQSize', cmdQsize)
        nic.addParam( 'baseAddr', nicBaseAddr )
        nic.addParam( 'pesPerNode', pesPerNode )
        
        memNIC = nic.setSubComponent("cpulink", "memHierarchy.MemNICFour")
        memNIC.addParams(shmemNic_nic_params)
        memdata = memNIC.setSubComponent("data", "kingsley.linkcontrol")
        memreq = memNIC.setSubComponent("req", "kingsley.linkcontrol")
        memack = memNIC.setSubComponent("ack", "kingsley.linkcontrol")
        memfwd = memNIC.setSubComponent("fwd", "kingsley.linkcontrol")
        memdata.addParams(data_net_params)
        memreq.addParams(ctrl_net_params)
        memfwd.addParams(ctrl_net_params)
        memack.addParams(ctrl_net_params)

        netLink = nic.setSubComponent( "rtrLink", "merlin.linkcontrol" )
        netLink.addParam("link_bw",link_bw)
        netLink.addParam("input_buf_size",input_buf_size)
        netLink.addParam("output_buf_size",output_buf_size)

        return (memreq, "rtr_port", mesh_link_latency), (memack, "rtr_port", mesh_link_latency), (memfwd, "rtr_port", mesh_link_latency), (memdata, "rtr_port", mesh_link_latency), netLink


nicBuilder = NicBuilder()

class Endpoint():
    def __init__( self, numNodes, pesPerNode, nicBaseAddr, cmdQsize, link_bw, input_buf_size, output_buf_size ):
        self.numNodes = numNodes
        self.pesPerNode = pesPerNode
        self.numPes = numNodes * pesPerNode
        self.nicBaseAddr = nicBaseAddr
        self.cmdQsize = cmdQsize
        self.link_bw = link_bw
        self.input_buf_size = input_buf_size
        self.output_buf_size = output_buf_size

        cpu_params['numPes'] = self.numPes
        cpu_params['cmdQSize'] = cmdQsize
        cpu_params['numNodes'] = numNodes
        cpu_params['threadsPerNode'] = pesPerNode 

    def prepParams( self ):
        pass

    def build( self, nicId, extraKeys ):

        tileBuilder.init(nicId,self.nicBaseAddr)
        memBuilder.init()
        DCBuilder.init(nicId)
        #print 'build node', nicId
        sst.pushNamePrefix( "nodeId" + str(nicId) )

        # Build Kingsley Mesh
        kRtrReq=[]
        kRtrAck=[]
        kRtrFwd=[]
        kRtrData=[]
        for x in range (0, mesh_stops_x):
            for y in range (0, mesh_stops_y):
                nodeNum = len(kRtrReq)
                kRtrReq.append(sst.Component("krtr_req_" + str(nodeNum), "kingsley.noc_mesh"))
                kRtrReq[-1].addParams(ctrl_network_params)
                kRtrAck.append(sst.Component("krtr_ack_" + str(nodeNum), "kingsley.noc_mesh"))
                kRtrAck[-1].addParams(ctrl_network_params)
                kRtrFwd.append(sst.Component("krtr_fwd_" + str(nodeNum), "kingsley.noc_mesh"))
                kRtrFwd[-1].addParams(ctrl_network_params)
                kRtrData.append(sst.Component("krtr_data_" + str(nodeNum), "kingsley.noc_mesh"))
                kRtrData[-1].addParams(data_network_params)
                
                kRtrReq[-1].addParams({"local_ports" : 2})
                kRtrAck[-1].addParams({"local_ports" : 2})
                kRtrFwd[-1].addParams({"local_ports" : 2})
                kRtrData[-1].addParams({"local_ports" : 2})

        i = 0
        for y in range(0, mesh_stops_y):
            for x in range (0, mesh_stops_x):
                #print( 'mesh stop x={0} y={1}').format(x,y) 

                # North-south connections
                if y != (mesh_stops_y -1):
                    #print( 'mesh stop x={0} y={1} connect north<->south').format(x,y) 
                    kRtrReqNS = sst.Link("krtr_req_ns_" + str(i))
                    kRtrReqNS.connect( (kRtrReq[i], "south", mesh_link_latency), (kRtrReq[i + mesh_stops_x], "north", mesh_link_latency) )
                    kRtrAckNS = sst.Link("krtr_ack_ns_" + str(i))
                    kRtrAckNS.connect( (kRtrAck[i], "south", mesh_link_latency), (kRtrAck[i + mesh_stops_x], "north", mesh_link_latency) )
                    kRtrFwdNS = sst.Link("krtr_fwd_ns_" + str(i))
                    kRtrFwdNS.connect( (kRtrFwd[i], "south", mesh_link_latency), (kRtrFwd[i + mesh_stops_x], "north", mesh_link_latency) )
                    kRtrDataNS = sst.Link("krtr_data_ns_" + str(i))
                    kRtrDataNS.connect( (kRtrData[i], "south", mesh_link_latency), (kRtrData[i + mesh_stops_x], "north", mesh_link_latency) )

                if x != (mesh_stops_x - 1):
                    #print( 'mesh stop x={0} y={1} connect east<->west').format(x,y) 
                    kRtrReqEW = sst.Link("krtr_req_ew_" + str(i))
                    kRtrReqEW.connect( (kRtrReq[i], "east", mesh_link_latency), (kRtrReq[i+1], "west", mesh_link_latency) )
                    kRtrAckEW = sst.Link("krtr_ack_ew_" + str(i))
                    kRtrAckEW.connect( (kRtrAck[i], "east", mesh_link_latency), (kRtrAck[i+1], "west", mesh_link_latency) )
                    kRtrFwdEW = sst.Link("krtr_fwd_ew_" + str(i))
                    kRtrFwdEW.connect( (kRtrFwd[i], "east", mesh_link_latency), (kRtrFwd[i+1], "west", mesh_link_latency) )
                    kRtrDataEW = sst.Link("krtr_data_ew_" + str(i))
                    kRtrDataEW.connect( (kRtrData[i], "east", mesh_link_latency), (kRtrData[i+1], "west", mesh_link_latency) )

                setNodeDist(i, kRtrReq[i], kRtrAck[i], kRtrFwd[i], kRtrData[i])
                i = i + 1

        req,ack,fwd,data,netLink = nicBuilder.build(nicId,self.numNodes, self.pesPerNode, self.nicBaseAddr, self.cmdQsize, self.link_bw, self.input_buf_size, self.output_buf_size)

        port = "north"
        
        rtrreqport = sst.Link("krtr_req_" + port + "_" +str(0))
        rtrreqport.connect( (kRtrReq[0], port, mesh_link_latency), req )
        rtrackport = sst.Link("krtr_ack_" + port + "_" + str(0))
        rtrackport.connect( (kRtrAck[0], port, mesh_link_latency), ack )
        rtrfwdport = sst.Link("krtr_fwd_" + port + "_" + str(0))
        rtrfwdport.connect( (kRtrFwd[0], port, mesh_link_latency), fwd )
        rtrdataport = sst.Link("kRtr_data_" + port + "_" + str(0))
        rtrdataport.connect( (kRtrData[0], port, mesh_link_latency), data )

        sst.popNamePrefix( )

        return (netLink,"rtr_port", '10ns')

#endpoint = Endpoint(0,0,0,0,0)
#endpoint.build(0,{})
