set pagination off
set confirm off
set breakpoint pending on
set logging file /captures/logs/goddess_kick_trace.log
set logging overwrite off
set logging redirect on
set logging on

# Raw message boundary for every node connected to Goddess. Keep a compact
# line for all traffic so an unexpected opcode is still discoverable; dump
# bytes only for the known kick/permit family and the observed outer 0x41.
break *0x0806c976
commands
  silent
  set $gd_node = *(void **)($esp + 4)
  set $gd_packet = *(unsigned char **)($esp + 8)
  set $gd_length = *(unsigned int *)($esp + 12)
  set $gd_opcode = *(unsigned char *)$gd_packet
  printf "[TRACE] ProcessMessage node=%p opcode=0x%02x length=%u\n", $gd_node, $gd_opcode, $gd_length
  if $gd_opcode == 0x29 || $gd_opcode == 0x31 || $gd_opcode == 0x41 || ($gd_opcode >= 0x8c && $gd_opcode <= 0x8f)
    x/64bx $gd_packet
    bt 6
  end
  continue
end

# CClientNode::Process after protocol validation and immediately before its
# member-function-table dispatch. This records the concrete handler selected.
break *0x0806c8ec
commands
  silent
  set $gd_packet = *(unsigned char **)($ebp + 12)
  set $gd_length = *(unsigned int *)($ebp + 16)
  set $gd_opcode = *(unsigned char *)$gd_packet
  if $gd_opcode == 0x29 || $gd_opcode == 0x31 || $gd_opcode == 0x41 || ($gd_opcode >= 0x8c && $gd_opcode <= 0x8f)
    printf "[TRACE] Dispatch opcode=0x%02x length=%u handler=%p object=%p\n", $gd_opcode, $gd_length, *(void **)($ebp - 20), $edx
  end
  continue
end

# Bishop registration helps identify which CClientNode pointer is the Bishop
# when correlating sender and receiver directions.
break *0x0806a8fe
commands
  silent
  printf "[TRACE] ClaimBishop node=%p packet=%p length=%u\n", *(void **)($esp + 4), *(void **)($esp + 8), *(unsigned int *)($esp + 12)
  bt 5
  continue
end

run
