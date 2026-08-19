set pagination off
set confirm off
set breakpoint pending on
set logging file /captures/logs/bishop_kick_trace.log
set logging overwrite off
set logging redirect on
set logging on

# Raw, decrypted client message boundary. Client opcode 0x41 is c2s_login;
# duplicate-login Confirm is encoded as a field inside this same 126-byte body.
break *0x0804f184
commands
  silent
  set $bp_player = *(unsigned char **)($esp + 4)
  set $bp_channel = *(unsigned int *)($esp + 8)
  set $bp_packet = *(unsigned char **)($esp + 12)
  set $bp_length = *(unsigned int *)($esp + 16)
  set $bp_opcode = *(unsigned char *)$bp_packet
  if $bp_opcode == 0x41
    printf "\n[TRACE] Client AppendData player=%p channel=%u opcode=0x%02x packet=%p length=%u state_ptr=%p state_value=%u\n", $bp_player, $bp_channel, $bp_opcode, $bp_packet, $bp_length, *(void **)($bp_player + 8), *(unsigned int *)($bp_player + 12)
    x/128bx $bp_packet
    bt 7
  end
  continue
end

break *0x0804f0f4
commands
  silent
  set $bp_player = *(unsigned char **)($esp + 4)
  set $bp_channel = *(unsigned int *)($esp + 8)
  set $bp_packet = *(unsigned char **)($esp + 12)
  set $bp_length = *(unsigned int *)($esp + 16)
  if *(unsigned char *)$bp_packet == 0x41
    printf "[TRACE] Client SmallPackProcess player=%p channel=%u packet=%p length=%u\n", $bp_player, $bp_channel, $bp_packet, $bp_length
  end
  continue
end

break *0x0804f09c
commands
  silent
  set $bp_player = *(unsigned char **)($esp + 4)
  set $bp_packet = *(unsigned char **)($esp + 8)
  set $bp_length = *(unsigned int *)($esp + 12)
  if *(unsigned char *)$bp_packet == 0x41
    printf "[TRACE] Client DispatchTaskForAccount player=%p opcode=0x41 packet=%p length=%u queue=%p\n", $bp_player, $bp_packet, $bp_length, $bp_player + 0x2c0
  end
  continue
end

# The observed client login frame arrives with owner/channel 2 and is routed
# through the per-player queue at CGamePlayer+0xAB8.
break *0x0804efa4
commands
  silent
  set $bp_player = *(unsigned char **)($esp + 4)
  set $bp_packet = *(unsigned char **)($esp + 8)
  set $bp_length = *(unsigned int *)($esp + 12)
  if *(unsigned char *)$bp_packet == 0x41
    printf "[TRACE] Client DispatchTaskForPlayer player=%p opcode=0x41 packet=%p length=%u queue=%p\n", $bp_player, $bp_packet, $bp_length, $bp_player + 0xab8
  end
  continue
end

# Boundary immediately before the player KMsgQueue::Push call.
break *0x0804efea
commands
  silent
  if *(unsigned int *)($esp + 4) == 0x41
    set $login_player = *(unsigned char **)($ebp + 8)
    set $login_packet = *(unsigned char **)($esp + 8)
    printf "[TRACE] Client player queue Push player=%p queue=%p task=%p task_stage=%u opcode=0x%02x packet=%p length=%u kick_flag_offset111=%u\n", $login_player, *(void **)($esp), $login_player + 0x2c, *(unsigned int *)($login_player + 0x3c), *(unsigned int *)($esp + 4), $login_packet, *(unsigned int *)($esp + 12), *(unsigned int *)($login_packet + 111)
  end
  continue
end

# KPlayerTask owns the login state machine at CGamePlayer+0x2c.  Log the
# command object, stage before execution, return code, and resulting stage.
# Return code 10 is the normal polling/no-message result and is suppressed to
# keep GDB from generating an unbounded log while clients are idle.
break *0x08074942
commands
  silent
  set $task = *(unsigned char **)($ebp + 8)
  set $task_player = *(void **)$task
  set $task_stage_before = *(unsigned int *)($task + 0x10)
  set $task_command = *(void **)($ebp - 12)
  set $task_result = $eax
  if $task_result != 10
    printf "[TRACE] KPlayerTask Execute task=%p player=%p stage_before=%u command=%p result=%u stage_current=%u\n", $task, $task_player, $task_stage_before, $task_command, $task_result, *(unsigned int *)($task + 0x10)
  end
  continue
end

# Stage 0 consumes external c2s_login (0x41).  Only log successful Pop calls;
# empty polling is already represented by KPlayerTask return code 10.
break *0x0804e95a
commands
  silent
  if $eax != 0
    set $stage_player = *(unsigned char **)($ebp + 8)
    printf "\n[TRACE] Stage WaitForAccPwd Pop player=%p service_type=%u task_stage=%u packet=%p length=%u kick_flag_offset111=%u\n", $stage_player, *(unsigned int *)($stage_player + 4), *(unsigned int *)($stage_player + 0x3c), $eax, *(unsigned int *)($ebp - 44), *(unsigned int *)($eax + 111)
    x/128bx $eax
  end
  continue
end

break *0x0804ec87
commands
  silent
  set $stage_result = *(unsigned int *)($ebp - 68)
  if $stage_result != 10
    printf "[TRACE] Stage WaitForAccPwd return player=%p result=%u\n", *(void **)($ebp + 8), $stage_result
  end
  continue
end

break *0x0804e7ac
commands
  silent
  printf "[TRACE] Stage TripLogin entry player=%p task_stage=%u account_state=%p\n", *(void **)($esp + 4), *(unsigned int *)(*(unsigned char **)($esp + 4) + 0x3c), *(void **)(*(unsigned char **)($esp + 4) + 8)
  continue
end

break *0x0804e924
commands
  silent
  printf "[TRACE] Stage TripLogin return player=%p result=%u\n", *(void **)($ebp + 8), *(unsigned int *)($ebp - 28)
  continue
end

break *0x0804e540
commands
  silent
  printf "[TRACE] Stage QueryAccPwd entry player=%p task_stage=%u account_state=%p\n", *(void **)($esp + 4), *(unsigned int *)(*(unsigned char **)($esp + 4) + 0x3c), *(void **)(*(unsigned char **)($esp + 4) + 8)
  continue
end

break *0x0804e7a2
commands
  silent
  printf "[TRACE] Stage QueryAccPwd return player=%p result=8\n", *(void **)($ebp + 8)
  continue
end

break *0x0804dca0
commands
  silent
  printf "[TRACE] Stage VerifyAccount entry player=%p task_stage=%u account_state=%p\n", *(void **)($esp + 4), *(unsigned int *)(*(unsigned char **)($esp + 4) + 0x3c), *(void **)(*(unsigned char **)($esp + 4) + 8)
  continue
end

break *0x0804e0b1
commands
  silent
  set $stage_result = *(unsigned int *)($ebp - 64)
  if $stage_result != 10
    printf "[TRACE] Stage VerifyAccount return player=%p result=%u account_state=%p\n", *(void **)($ebp + 8), $stage_result, *(void **)(*(unsigned char **)($ebp + 8) + 8)
  end
  continue
end

# Final GameServer permit boundary. The player task waits for internal opcode
# 0x34, validates the account/map, reads bPermit at packet offset 0x35, then
# forwards the same structure to the external client.
break *0x0804c740
commands
  silent
  set $permit_player = *(unsigned char **)($esp + 4)
  printf "\n[TRACE] WaitForGameSvrPermit entry player=%p connection_index=%u account_state_ptr=%p\n", $permit_player, *(unsigned int *)($permit_player + 12), *(void **)($permit_player + 8)
  continue
end

break *0x0804c76c
commands
  silent
  printf "[TRACE] WaitForGameSvrPermit Pop(0x34) packet=%p length=%u\n", $eax, *(unsigned int *)($ebp - 16)
  if $eax != 0
    x/54bx $eax
  end
  continue
end

# Return from the virtual packet/account validation call.
break *0x0804c7b1
commands
  silent
  printf "[TRACE] WaitForGameSvrPermit packet validation returned=%u packet=%p\n", $eax, *(void **)($ebp - 4)
  continue
end

# Return from CheckAccountMap().
break *0x0804c7d6
commands
  silent
  printf "[TRACE] WaitForGameSvrPermit CheckAccountMap returned=%u\n", $eax
  continue
end

# Exact final send to the Windows client from WaitForGameSvrPermit.
break *0x0804c819
commands
  silent
  set $permit_packet = *(unsigned char **)($esp + 4)
  set $permit_length = *(unsigned int *)($esp + 8)
  printf "[TRACE] WaitForGameSvrPermit SendData player=%p packet=%p length=%u permit=%u\n", *(void **)($esp), $permit_packet, $permit_length, *(unsigned char *)($permit_packet + 0x35)
  x/54bx $permit_packet
  continue
end

break *0x0804c824
commands
  silent
  printf "[TRACE] WaitForGameSvrPermit return_code=%u\n", *(unsigned int *)($ebp - 20)
  continue
end

# Raw GameServer-to-Bishop notify handler that should enqueue opcode 0x34.
break *0x08057c16
commands
  silent
  set $notify_packet = *(unsigned char **)($esp + 8)
  set $notify_length = *(unsigned int *)($esp + 12)
  printf "\n[TRACE] GameServer NotifyPlayerLogin server=%p packet=%p length=%u opcode=0x%02x\n", *(void **)($esp + 4), $notify_packet, $notify_length, *(unsigned char *)$notify_packet
  x/54bx $notify_packet
  bt 6
  continue
end

# Generic external send boundary, restricted to the permit structure.
break *0x0804b7ba
commands
  silent
  set $send_packet = *(unsigned char **)($esp + 8)
  set $send_length = *(unsigned int *)($esp + 12)
  if $send_packet != 0 && *(unsigned char *)$send_packet == 0x34
    printf "[TRACE] Client SendData permit player=%p connection_index=%u packet=%p length=%u permit=%u\n", *(void **)($esp + 4), *(unsigned int *)(*(unsigned char **)($esp + 4) + 12), $send_packet, $send_length, *(unsigned char *)($send_packet + 0x35)
  end
  continue
end

# Boundary immediately before KMsgQueue::Push(opcode, packet, length).
break *0x0804f0e2
commands
  silent
  if *(unsigned int *)($esp + 4) == 0x41
    printf "[TRACE] Client account queue Push queue=%p opcode=0x%02x packet=%p length=%u\n", *(void **)($esp), *(unsigned int *)($esp + 4), *(void **)($esp + 8), *(unsigned int *)($esp + 12)
  end
  continue
end

break *0x0804e452
commands
  silent
  printf "\n[TRACE] Stage AskOfflinePermission entry player=%p task_stage=%u account_state=%p\n", *(void **)($esp + 4), *(unsigned int *)(*(unsigned char **)($esp + 4) + 0x3c), *(void **)(*(unsigned char **)($esp + 4) + 8)
  x/8wx $esp
  bt 8
  continue
end

break *0x0804e537
commands
  silent
  printf "[TRACE] Stage AskOfflinePermission return player=%p result=%u account_state=%p\n", *(void **)($ebp + 8), *(unsigned int *)($ebp - 148), *(void **)(*(unsigned char **)($ebp + 8) + 8)
  continue
end

break *0x0804e104
commands
  silent
  if $eax != 0
    printf "[TRACE] Stage WaitForOfflineResult Pop player=%p packet=%p length=%u result_flag=%u\n", *(void **)($ebp + 8), $eax, *(unsigned int *)($ebp - 24), *(unsigned char *)($eax + 37)
    x/42bx $eax
  end
  continue
end

break *0x0804e340
commands
  silent
  set $stage_result = *(unsigned int *)($ebp - 312)
  if $stage_result != 10
    printf "[TRACE] Stage WaitForOfflineResult return player=%p result=%u\n", *(void **)($ebp + 8), $stage_result
  end
  continue
end

# CGamePlayer::KickOfflineLock(Account const*) entry. Account fields used by
# this function are +0x08 (map/player value copied into the request) and
# +0x0c (GameServer id passed to CIntercessor::QueryServer()).
break *0x0804e348
commands
  silent
  set $kick_player = *(void **)($esp + 4)
  set $kick_account = *(unsigned char **)($esp + 8)
  printf "\n[TRACE] KickOfflineLock entry player=%p account=%p request_value=%u server_id=%u\n", $kick_player, $kick_account, *(unsigned int *)($kick_account + 8), *(unsigned int *)($kick_account + 12)
  x/16bx $kick_account
  bt 6
  continue
end

# Instruction immediately following QueryServer(server_id).
break *0x0804e367
commands
  silent
  set $kick_account = *(unsigned char **)($ebp + 12)
  printf "[TRACE] KickOfflineLock QueryServer server_id=%u result=%p\n", *(unsigned int *)($kick_account + 12), $eax
  continue
end

# DispatchTask(0x0b, request, 0x29, NULL) call boundary.
break *0x0804e40a
commands
  silent
  set $kick_packet = *(unsigned char **)($esp + 8)
  printf "[TRACE] KickOfflineLock DispatchTask server=%p task=%u packet=%p length=%u context=%p\n", *(void **)($esp), *(unsigned int *)($esp + 4), $kick_packet, *(unsigned int *)($esp + 12), *(void **)($esp + 16)
  x/41bx $kick_packet
  continue
end

break *0x0804e40f
commands
  silent
  printf "[TRACE] KickOfflineLock DispatchTask returned=%u\n", $eax
  continue
end

break *0x08056670
commands
  silent
  set $packet = *(unsigned char **)($esp + 8)
  set $length = *(unsigned int *)($esp + 12)
  printf "\n[TRACE] OfflineKickoutRes this=%p packet=%p length=%u\n", *(void **)($esp + 4), $packet, $length
  x/42bx $packet
  bt 8
  continue
end

run
