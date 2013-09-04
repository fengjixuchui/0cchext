#include "stdafx.h"
#include "0cchext.h"
#include "util.h"
#include <engextcpp.hpp>

class EXT_CLASS : public ExtExtension
{
public:
	EXT_COMMAND_METHOD(hwnd);
	EXT_COMMAND_METHOD(setvprot);
	EXT_COMMAND_METHOD(dpx);
};

EXT_DECLARE_GLOBALS();


EXT_COMMAND(hwnd,
	"Show window information by handle.",
	"{;ed,r;hwnd;A handle to the window}")
{
	ULONG class_type = 0, qualifier_type = 0;
	HRESULT hr = m_Control->GetDebuggeeType(&class_type, &qualifier_type);
	if (FAILED(hr)) {
		Err("Failed to get debuggee type\n");
		return;
	}

	if (class_type != DEBUG_CLASS_KERNEL) {
		Err("This command must be used in Kernel-Mode\n");
		return;
	}

	ULONG64 wnd_handle = GetUnnamedArgU64(0);
	DEBUG_VALUE dbg_value = {0};
	hr = m_Control->Evaluate("win32k!gSharedInfo", DEBUG_VALUE_INT64, &dbg_value, NULL);
	if (FAILED(hr)) {
		Err("Failed to get win32k!gSharedInfo\n");
		return;
	}

	ExtRemoteTyped shared_info("(win32k!tagSHAREDINFO *)@$extin", dbg_value.I64);
	ULONG handle_Count = shared_info.Field("psi.cHandleEntries").GetUlong();

	if ((wnd_handle & 0xffff) >= handle_Count) {
		Err("Invalidate window handle value.\n");
		return;
	}

	ULONG entry_size = shared_info.Field("HeEntrySize").GetUlong();
	ULONG64 entries = shared_info.Field("aheList").GetUlongPtr();
	ULONG64 target_entry = entries + entry_size * (wnd_handle & 0xffff);
	ExtRemoteData wnd_data(target_entry, sizeof(PVOID));

	ExtRemoteTyped wnd_ptr("(win32k!tagWnd *)@$extin", wnd_data.GetPtr());
	Out("HWND: %p\n", wnd_ptr.Field("head.h").GetPtr());
	Dml("tagWnd * @ <link cmd=\"dt %p win32k!tagWnd\">%p</link>\n", wnd_data.GetPtr(), wnd_data.GetPtr());

	if (wnd_ptr.Field("strName.Buffer").GetPtr() != 0) {
		Out("Window Name: %mu\n", wnd_ptr.Field("strName.Buffer").GetPtr());
	}

	Dml("tagCLS * @ <link cmd=\"r @$t0=%p;dt @@C++(((win32k!tagWnd *)@$t0)->pcls) win32k!tagCLS\">%p</link>\n", 
		wnd_data.GetPtr(), wnd_ptr.Field("pcls").GetPtr());

	if (wnd_ptr.Field("pcls.lpszAnsiClassName").GetPtr() != 0) {
		Out("Window Class Name: %ma\n", wnd_ptr.Field("pcls.lpszAnsiClassName").GetPtr());
	}
	if (wnd_ptr.Field("spwndNext").GetPtr() != 0) {
		Dml("Next Wnd:     <link cmd=\"!0cchext.hwnd %p\">%p</link>\n", 
			wnd_ptr.Field("spwndNext.head.h").GetPtr(), wnd_ptr.Field("spwndNext.head.h").GetPtr());
	}
	if (wnd_ptr.Field("spwndPrev").GetPtr() != 0) {
		Dml("Previous Wnd: <link cmd=\"!0cchext.hwnd %p\">%p</link>\n", 
			wnd_ptr.Field("spwndPrev.head.h").GetPtr(), wnd_ptr.Field("spwndPrev.head.h").GetPtr());
	}
	if (wnd_ptr.Field("spwndParent").GetPtr() != 0) {
		Dml("Parent Wnd:   <link cmd=\"!0cchext.hwnd %p\">%p</link>\n", 
			wnd_ptr.Field("spwndParent.head.h").GetPtr(), wnd_ptr.Field("spwndParent.head.h").GetPtr());
	}
	if (wnd_ptr.Field("spwndChild").GetPtr() != 0) {
		Dml("Child Wnd:    <link cmd=\"!0cchext.hwnd %p\">%p</link>\n", 
			wnd_ptr.Field("spwndChild.head.h").GetPtr(), wnd_ptr.Field("spwndChild.head.h").GetPtr());
	}
	if (wnd_ptr.Field("spwndOwner").GetPtr() != 0) {
		Dml("Own Wnd:      <link cmd=\"!0cchext.hwnd %p\">%p</link>\n", 
			wnd_ptr.Field("spwndOwner.head.h").GetPtr(), wnd_ptr.Field("spwndOwner.head.h").GetPtr());
	}
	if (wnd_ptr.Field("lpfnWndProc").GetPtr() != 0) {
		Dml("pfnWndProc:   "
			"<link cmd=\"r @$t0=%p;.process /p /r @@C++(((nt!_ETHREAD *)((win32k!tagWnd *)@$t0)->head.pti->pEThread)->Tcb.Process);"
			"u @@C++(((win32k!tagWnd *)@$t0)->lpfnWndProc)\">%p</link>\n", 
			wnd_data.GetPtr(), wnd_ptr.Field("lpfnWndProc").GetPtr());
	}

	ULONG style = wnd_ptr.Field("style").GetUlong();

	Out("Visible:  %d\n", (style & (1<<28)) != 0);
	Out("Child:    %d\n", (style & (1<<30)) != 0);
	Out("Minimized:%d\n", (style & (1<<29)) != 0);
	Out("Disabled: %d\n", (style & (1<<27)) != 0);
	Out("Window Rect {%d, %d, %d, %d}\n", 
		wnd_ptr.Field("rcWindow.left").GetLong(),
		wnd_ptr.Field("rcWindow.top").GetLong(),
		wnd_ptr.Field("rcWindow.right").GetLong(),
		wnd_ptr.Field("rcWindow.bottom").GetLong());
	Out("Clent Rect  {%d, %d, %d, %d}\n",
		wnd_ptr.Field("rcClient.left").GetLong(),
		wnd_ptr.Field("rcClient.top").GetLong(),
		wnd_ptr.Field("rcClient.right").GetLong(),
		wnd_ptr.Field("rcClient.bottom").GetLong());

	Out("\n");
}

EXT_COMMAND(setvprot,
	"Set the protection on a region of committed pages in the virtual address space of the debuggee process.",
	"{;ed,r;Address;Base address of the region of pages}"
	"{;ed,r;Size;The size of the region}"
	"{;ed,r;type;The new protection type}"
	)
{
	ULONG class_type = 0, qualifier_type = 0;
	HRESULT hr = m_Control->GetDebuggeeType(&class_type, &qualifier_type);
	if (FAILED(hr)) {
		Err("Failed to get debuggee type\n");
		return;
	}

	if (class_type != DEBUG_CLASS_USER_WINDOWS) {
		Err("This command must be used in User-Mode\n");
		return;
	}

	ULONG64 base_address = GetUnnamedArgU64(0);
	ULONG64 region_size = GetUnnamedArgU64(1);
	ULONG64 protection_type = GetUnnamedArgU64(2);

	ULONG64 handle = 0;
	hr = m_System->GetCurrentProcessHandle(&handle);
	if (FAILED(hr)) {
		Err("Failed to get process handle.\n");
		return;
	}

	ULONG old_type = 0;
	if (!VirtualProtectEx((HANDLE)handle, 
		(PVOID)base_address, 
		(SIZE_T)region_size, 
		(ULONG)protection_type, 
		&old_type)) {
			Err("Failed to set virtual protection type.\n");
			return;
	}

	Dml("[%p - %p] Change %08X to %08X <link cmd=\"!vprot %p\">Detail</link>\n",
		base_address, region_size, (ULONG)old_type, (ULONG)protection_type, base_address);
}

EXT_COMMAND(dpx,
	"Display the contents of memory in the given range.",
	"{;ed,r;Address;Base address of the memory area to display}"
	"{;ed,o,d=10;range;The range of the memory area}"
	)
{
	ULONG64 base_address = GetUnnamedArgU64(0);
	ULONG64 range = GetUnnamedArgU64(1);

	ExtRemoteData base_data;
	ULONG64 query_data;
	CHAR buffer[128];
	ULONG ret_size = 0;
	ULONG64 displacement = 0;
	ULONG print_flag = 0;

	for (ULONG64 i = 0; i < range; i++) {
		base_data.Set(base_address + i * sizeof(PVOID), sizeof(PVOID));
		query_data = base_data.GetPtr();
		ret_size = 0;
		ZeroMemory(buffer, sizeof(buffer));
		print_flag = 0;

		if (SUCCEEDED(m_Symbols->GetNameByOffset(query_data, 
			buffer, 
			sizeof(buffer), 
			&ret_size, 
			&displacement))) {
				print_flag |= 1;
		}
		
		if (m_Data4->ReadUnicodeStringVirtual(query_data, 
			0x1000, 
			CP_ACP,
			buffer, 
			sizeof(buffer), 
			&ret_size) != E_INVALIDARG && 
			strlen(buffer) != 0 &&
			IsPrintAble(buffer, strlen(buffer))) {
				print_flag |= 2;
		}
		else if (m_Data4->ReadMultiByteStringVirtual(query_data, 
			0x1000, 
			buffer, 
			sizeof(buffer), 
			&ret_size) != E_INVALIDARG && 
			strlen(buffer) != 0 &&
			IsPrintAble(buffer, strlen(buffer))) {
				print_flag |= 4;
		}

		if (print_flag == 0) {
			Dml("%p  %p\n", base_address + i * sizeof(PVOID), query_data);
		}
		else {
			Dml("%p  %p", base_address + i * sizeof(PVOID), query_data);
			if (print_flag & 1) {
				Dml("  [S] %ly", query_data);
			}

			if (print_flag & 2) {
				Dml("  [U] \"%mu\"", query_data);
			}

			if (print_flag & 4) {
				Dml("  [A] \"%ma\"", query_data);
			}

			Dml("\n");
		}
	}
	
}