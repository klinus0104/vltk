set pagination off
set confirm off
set breakpoint pending on
set logging file /captures/logs/gameserver_kick_trace.log
set logging overwrite off
set logging redirect on
set logging on

break *0x081ed120
commands
  silent
  set $packet = *(unsigned char **)($esp + 8)
  set $length = *(unsigned int *)($esp + 12)
  printf "\n[TRACE] ProcessOfflineKickout this=%p packet=%p length=%u\n", *(void **)($esp + 4), $packet, $length
  x/41bx $packet
  bt 8
  continue
end

break *0x081ecea0
commands
  silent
  set $packet = *(unsigned char **)($esp + 8)
  printf "\n[TRACE] OfflineKickout this=%p packet=%p\n", *(void **)($esp + 4), $packet
  x/41bx $packet
  bt 8
  continue
end

break *0x081ecf70
commands
  silent
  set $packet = *(unsigned char **)($esp + 8)
  set $length = *(unsigned int *)($esp + 12)
  printf "\n[TRACE] OfflineKickout sending response packet=%p length=%u\n", $packet, $length
  x/42bx $packet
  bt 8
  continue
end

run
