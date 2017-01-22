

function sendAlert (lvl, dest, title, message)
	local lvltext =""
	if lvl == critical then
		lvltext = "critical"
	elseif lvl == error then
		lvltext = "error"
	elseif lvl == warning then
		lvltext = "warning"
	else
		return -- do nothing with lvl==info alerts
	end
	mail = io.popen("mail -s '"..title.."' "..dest, "w")
	mail:write(message.."\n\4")
	mail:close()
end
