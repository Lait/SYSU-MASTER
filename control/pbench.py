import wx
from wx.lib.wordwrap import wordwrap

import server
import config
import test

TBFLAGS = ( wx.TB_HORIZONTAL
            | wx.NO_BORDER
            | wx.TB_FLAT
            #| wx.TB_TEXT
            #| wx.TB_HORZ_LAYOUT
            )

class PBench(wx.Frame):
	def __init__(self, parent, id, title, 
		pos=wx.DefaultPosition, size=wx.DefaultSize, 
		style=wx.DEFAULT_FRAME_STYLE):
		wx.Frame.__init__(self, parent, id, title, pos, size, style)
		#self.__main_panel = wx.Panel(self, -1)

		self.__operations = {
			1101:self.on_exit, 
			1301:self.on_about,

			2101:self.on_start,
			2102:self.on_reset_params,
			2103:self.on_add_server
		}

		self.__createMenuBar()
		self.__createToolBar()
		self.__createParamsBar()

		self.__bindEvents()

	def __bindEvents(self):
		self.Bind(wx.EVT_CLOSE, self.on_destroy)
		

	def __createMenuBar(self):
		menuBar = wx.MenuBar()

		menu1_file = wx.Menu()
		menuBar.Append(menu1_file, "&File")
		menu1_file.Append(1101, "Exit")

		menu2_edit = wx.Menu()
		menuBar.Append(menu2_edit, "&Edit")

		menu3_help = wx.Menu()
		menuBar.Append(menu3_help, "&Help")
		menu3_help.Append(1301, "About")

		self.SetMenuBar(menuBar)
		self.Bind(wx.EVT_MENU, self.event_dispatch)


	def __createToolBar(self):
		self.__tb = self.CreateToolBar()

		tsize = (24,24)
		start_test_bmp = wx.ArtProvider.GetBitmap(wx.ART_GO_FORWARD, wx.ART_TOOLBAR, tsize)
		reset_bmp      = wx.ArtProvider.GetBitmap(wx.ART_REDO, wx.ART_TOOLBAR, tsize)
		add_server_bmp = wx.ArtProvider.GetBitmap(wx.ART_NEW, wx.ART_TOOLBAR, tsize)

		self.__tb.SetToolBitmapSize(tsize)
		self.__tb.AddLabelTool(2101, "Start", start_test_bmp, shortHelp="Start", longHelp="Start test.")
		self.__tb.AddLabelTool(2102, "Reset", reset_bmp, shortHelp="Reset", longHelp="Reset params.")
		self.__tb.AddLabelTool(2103, "Add", add_server_bmp, shortHelp="Add", longHelp="Add server.")

		self.Bind(wx.EVT_TOOL, self.event_dispatch)
		self.Bind(wx.EVT_TOOL_RCLICKED, self.event_dispatch)

		# Final thing to do for a toolbar is call the Realize() method. This
		# causes it to render (more or less, that is).
		self.__tb.Realize()

	def __createParamsBar(self):
		pass

	def event_dispatch(self, event):
		# CONFUSING: All wx.EVT_MENU event will be recognized as wx.EVT_TOOL, wierd....
		id = event.GetId()
		print id
		if (self.__operations.has_key(id)):
			self.__operations[id](event)

	def on_start(self, event):
		print "On start"

	def on_add_server(self, event):
		print "On add server"

	def on_reset_params(self, event):
		print "on_reset_params"

	def on_exit(self, event):
		print "On Exit"
		self.Close(True)

	def on_about(self, event):
		# First we create and fill the info object
		info = wx.AboutDialogInfo()
		info.Name = "PBench"
		info.Version = "0.0.1"
		info.Copyright = "(C) 2014 SYSU"
		info.Description = wordwrap(
		   "This is a benchmark tool base on OpenMPI.",
		   350, wx.ClientDC(self))
		info.WebSite = ("https://github.com/Lait/SYSU-MASTER", "PBench home")
		info.Developers = [ "Leon Lai"]
		info.License = wordwrap(
			"Just use it, do not asking questions...",
			500, wx.ClientDC(self))

		# Then we call wx.AboutBox giving it that info object
		wx.AboutBox(info)

	def on_destroy(self, event):
		print "OnCloseWindow"
		self.Destroy()

def main():
	app = wx.App(False)
	frame = PBench(None, wx.ID_ANY, "PBench")
	frame.Show(True)
	app.MainLoop()

if __name__ == '__main__':
	main()
