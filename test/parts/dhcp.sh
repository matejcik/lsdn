function dhcp_server(){
	in_virt $1 $2 tcpdump -i out -w dump.pcap&
#	cat << EOF > /tmp/dhcpd.conf
#default-lease-time 600;
#max-lease-time 7200;
#
#subnet 192.168.99.0 netmask 255.255.255.0 {
#	option subnet-mask 255.255.255.0;
#	range 192.168.99.2 192.168.99.250;
#}
## Having a statically assigned addresses prevent the DHCP server from doing ARP probes
## and speeds up the whole process
#host b1 { hardware ethernet 00:00:00:00:00:b1; fixed-address 192.168.99.2; }
#host b2 { hardware ethernet 00:00:00:00:00:b2; fixed-address 192.168.99.3; }
#host b3 { hardware ethernet 00:00:00:00:00:b3; fixed-address 192.168.99.5; }
#host c1 { hardware ethernet 00:00:00:00:00:c1; fixed-address 192.168.99.4; }
#EOF

# TODO This is workaround until rkapl's patch for 9p on COW FS is accepted. The problem is that cat
# cannot open stdin thus fails when using HEREDOC.
echo "default-lease-time 600;" > /tmp/dhcpd.conf
echo "max-lease-time 7200;" >> /tmp/dhcpd.conf
echo "subnet 192.168.99.0 netmask 255.255.255.0 {" >> /tmp/dhcpd.conf
echo "	option subnet-mask 255.255.255.0;" >> /tmp/dhcpd.conf
echo "	range 192.168.99.2 192.168.99.250;" >> /tmp/dhcpd.conf
echo "}" >> /tmp/dhcpd.conf
echo "host b1 { hardware ethernet 00:00:00:00:00:b1; fixed-address 192.168.99.2; }" >> /tmp/dhcpd.conf
echo "host b2 { hardware ethernet 00:00:00:00:00:b2; fixed-address 192.168.99.3; }" >> /tmp/dhcpd.conf
echo "host b3 { hardware ethernet 00:00:00:00:00:b3; fixed-address 192.168.99.5; }" >> /tmp/dhcpd.conf
echo "host c1 { hardware ethernet 00:00:00:00:00:c1; fixed-address 192.168.99.4; }" >> /tmp/dhcpd.conf
# TODO END


	echo > /tmp/dhcpd.leases
	in_virt $1 $2 dhcpd -4 -cf /tmp/dhcpd.conf -lf /tmp/dhcpd.leases --no-pid $3
}

function dhcp_client(){
	rm /var/lib/dhcpcd/dhcpcd-*.lease || true
	# -A disables ARP probes and makes the lease quicker
	in_virt $1 $2 dhcpcd -A -4 --oneshot -t 5 $3
}

function prepare(){
	mk_testnet net
	mk_phys net a ip 172.16.0.1/24
	mk_phys net b ip 172.16.0.2/24
	mk_phys net c ip 172.16.0.3/24

	mk_virt a 1 ip 192.168.99.1/24 mac 00:00:00:00:00:a1
	mk_virt b 1 mac 00:00:00:00:00:b1
	mk_virt b 2 mac 00:00:00:00:00:b2
	mk_virt b 3 mac 00:00:00:00:00:b3
	mk_virt c 1 mac 00:00:00:00:00:c1

	mk_bridge net switch a b c
}

function connect(){
	lsctl_in_all_phys parts/dhcp.lsctl

	dhcp_server a 1 out

	pass dhcp_client b 1 out
	pass dhcp_client b 2 out
	fail dhcp_client b 3 out
	pass dhcp_client c 1 out

}

function test(){
	pass in_virt b 1 $qping 192.168.99.1
	pass in_virt b 1 $qping 192.168.99.2
	pass in_virt b 1 $qping 192.168.99.3
	pass in_virt b 1 $qping 192.168.99.4
	fail in_virt b 1 $qping 192.168.99.5
	fail in_virt b 3 $qping 192.168.99.2
}
