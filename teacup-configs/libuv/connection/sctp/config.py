
import sys
import datetime
from fabric.api import env

env.user = 'root'
env.password = 'toor'
env.shell = '/bin/sh -c'

TPCONF_script_path = '/home/teacup/fredhau/git/teacup-nd'
sys.path.append(TPCONF_script_path)

TPCONF_debug_level = 0
TPCONF_pcap_snaplen = 400

TPCONF_router = ['router', ]
TPCONF_hosts = [ 'testhost2', 'testhost5', ]

TPCONF_host_internal_ip = {
    'router': ['172.16.10.254', '172.16.11.254'],
    'testhost2':  ['172.16.10.2'],
    'testhost5':  ['172.16.11.4'],
}

TPCONF_max_time_diff = 1

now = datetime.datetime.today()
TPCONF_test_id = 'noprefix' + '_libuvconnectionsctp'

TPCONF_remote_dir = '/tmp/'
TPCONF_runs = 1

TPCONF_router_queues = [
    # Set same delay for every host
    ('1', " source='172.16.10.0/24', dest='172.16.11.0/24', delay=V_delay, "
     " loss=V_loss, rate=V_up_rate, queue_disc=V_aqm, queue_size=V_bsize "),
    ('2', " source='172.16.11.0/24', dest='172.16.10.0/24', delay=V_delay, "
     " loss=V_loss, rate=V_down_rate, queue_disc=V_aqm, queue_size=V_bsize "),
]

traffic_custom = [
    ('0.0', '1', " start_custom_traffic,"
     " name='libuv_server',"
     " directory='/usr/home/teacup/fredhau/git/neat-test-suite/build',"
     " hostname='testhost2',"
     " duration=V_duration,"
     " add_prefix='1',"
     " parameters='-R %s -A -s -C %s -a %s -b %s -I %s -M %s -p %s -u %s' % ('fredhau/testhost2-freebsd/libuv/connection/sctp', str(int(V_connections) * int(V_flows)), '0', V_flows, '172.16.10.2', V_transports, 12327, 2)"),
    ('2.0', '2', " start_custom_traffic,"
     " name='libuv_client',"
     " directory='/usr/home/teacup/fredhau/git/neat-test-suite/build',"
     " hostname='testhost5',"
     " duration=V_duration,"
     " add_prefix='1',"
     " parameters='-R %s -A -s -C %s -a %s -b %s -i %s -l %s -M %s -n %s -u %s %s %s' % ('fredhau/testhost5-freebsd/libuv/connection/sctp', V_flows, '0', V_flows, '172.16.11.4', 10240, V_transports, V_flows, 2, '172.16.10.2', 12327)"),
]

TPCONF_traffic_gens = traffic_custom

#TPCONF_custom_loggers = [
#    ('6', " start_custom_logger,"
#     " name='data_sampler.sh',"
#     " logname='datasampler',"
#     " directory='/usr/home/teacup/neat/neat-performance/scripts',"
#     " hostname='testhost1',"
#     " parameters='neat_server neat_server 10 100'"),
#]

TPCONF_duration = 4

TPCONF_TCP_algos = ['newreno', ]

TPCONF_host_TCP_algos = {
}

TPCONF_host_TCP_algo_params = {
}

TPCONF_host_init_custom_cmds = {
'testhost2' : [ 'sysctl net.inet.tcp.recvbuf_auto=0',
                'sysctl net.inet.tcp.sendbuf_auto=0',
                'sysctl net.inet.tcp.sendspace=300000',
                'sysctl net.inet.tcp.recvspace=300000',
                'sysctl net.inet.sctp.sendspace=300000',
                'sysctl net.inet.sctp.recvspace=300000',
		'sysctl net.inet.tcp.msl=5000' ],
'testhost5' : [ 'sysctl net.inet.tcp.recvbuf_auto=0',
                'sysctl net.inet.tcp.sendbuf_auto=0',
                'sysctl net.inet.tcp.sendspace=300000',
                'sysctl net.inet.tcp.recvspace=300000',
                'sysctl net.inet.sctp.sendspace=300000',
                'sysctl net.inet.sctp.recvspace=300000',
                'sysctl net.inet.ip.portrange.randomized=0',
		'sysctl net.inet.tcp.msl=5000' ]
}

TPCONF_delays = [50]

TPCONF_loss_rates = [0]

TPCONF_bandwidths = [
    ('10mbit', '10mbit'),
]

TPCONF_aqms = ['bfifo', ]

TPCONF_buffer_sizes = ['bdp']

TPCONF_data_sizes = [123]

TPCONF_flows = [256]
#TPCONF_flows = [1, 2, 4, 8, 16, 32, 64, 128, 256]

TPCONF_transports = [('SCTP', '1')]
#TPCONF_transports = [('TCP', '1'), ('SCTP', '1'), ('SCTP-TCP', '2')]

TPCONF_parameter_list = {
#   Vary name		V_ variable	  file name	values			extra vars
    'delays' 	    :  (['V_delay'], 	  ['del'], 	TPCONF_delays, 		 {}),
    'loss'  	    :  (['V_loss'], 	  ['loss'], 	TPCONF_loss_rates, 	 {}),
    'tcpalgos' 	    :  (['V_tcp_cc_algo'],['tcp'], 	TPCONF_TCP_algos, 	 {}),
    'aqms'	    :  (['V_aqm'], 	  ['aqm'], 	TPCONF_aqms, 		 {}),
    'bsizes'	    :  (['V_bsize'], 	  ['bs'], 	TPCONF_buffer_sizes, 	 {}),
    'runs'	    :  (['V_runs'],       ['run'], 	range(TPCONF_runs), 	 {}),
    'bandwidths'    :  (['V_down_rate', 'V_up_rate'], ['down', 'up'], TPCONF_bandwidths, {}),
    'data_sizes'    :  (['V_data_size'], ['dsize'], TPCONF_data_sizes, {}),
    'flows'         :  (['V_flows'], ['flows'], TPCONF_flows, {}),
    'transports'    :  (['V_transports', 'V_connections'], ['transports', 'connectionsperflow'], TPCONF_transports,  {}),
}

TPCONF_variable_defaults = {
#   V_ variable			value
    'V_duration'  	:	TPCONF_duration,
    'V_delay'  		:	TPCONF_delays[0],
    'V_loss'   		:	TPCONF_loss_rates[0],
    'V_tcp_cc_algo' 	:	TPCONF_TCP_algos[0],
    'V_down_rate'   	:	TPCONF_bandwidths[0][0],
    'V_up_rate'	    	:	TPCONF_bandwidths[0][1],
    'V_aqm'	    	:	TPCONF_aqms[0],
    'V_bsize'	    	:	TPCONF_buffer_sizes[0],
    'V_data_size'       :       TPCONF_data_sizes[0],
    'V_flows'           :       TPCONF_flows[0],
    'V_transports'      :       TPCONF_transports[0][0],
    'V_connections'     :       TPCONF_transports[0][1],
}

TPCONF_vary_parameters = ['flows', 'data_sizes', 'transports', 'runs']
