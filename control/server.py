import config

class Server:
	def __init__(self, ip, port, ssh_user, ssh_pw, ssh_port, hardware_info):
		self.ip = ip
		self.port = port
		self.ssh_user = ssh_user
		self.ssh_pw = ssh_pw
		self.ssh_port = ssh_port
		self.hardware_info = hardware_info

active_servers = []







