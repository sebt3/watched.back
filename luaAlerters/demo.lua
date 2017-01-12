

function sendAlert (lvl, dest, title, message)
	local lvltext =""
	if lvl == critical then
		lvltext = "critical"
	elseif lvl == error then
		lvltext = "error"
	elseif lvl == warning then
		lvltext = "warning"
	elseif lvl == notice then
		lvltext = "notice"
	else
		return -- do nothing with lvl==info alerts
	end
	print("luaDemoSendAlert", dest, title, message)
	-- one can use io.popen("command") to start a shell command instead
end
