// simplewall
// Copyright (c) 2016-2020 Henry++

#include "global.hpp"

void _app_timer_set (HWND hwnd, PITEM_APP ptr_app, time_t seconds)
{
	if (!ptr_app)
		return;

	const size_t app_hash = _r_str_hash (ptr_app->original_path); // note: be carefull (!)

	const INT listview_id = _app_getlistview_id (ptr_app->type);
	const INT item_pos = _app_getposition (hwnd, listview_id, app_hash);

	if (seconds <= 0)
	{
		ptr_app->is_enabled = false;
		ptr_app->is_haveerrors = false;

		ptr_app->timer = 0;

		if (ptr_app->htimer)
		{
			DeleteTimerQueueTimer (config.htimer, ptr_app->htimer, nullptr);
			ptr_app->htimer = nullptr;
		}
	}
	else
	{
		const time_t current_time = _r_unixtime_now ();
		bool is_created = false;

		if (ptr_app->htimer)
		{
			is_created = !!ChangeTimerQueueTimer (config.htimer, ptr_app->htimer, DWORD (seconds * _R_SECONDSCLOCK_MSEC), 0);
		}
		else
		{
			is_created = !!CreateTimerQueueTimer (&ptr_app->htimer, config.htimer, &_app_timer_callback, (PVOID)app_hash, DWORD (seconds * _R_SECONDSCLOCK_MSEC), 0, WT_EXECUTEONLYONCE | WT_EXECUTEINTIMERTHREAD);
		}

		if (is_created)
		{
			ptr_app->is_enabled = true;
			ptr_app->timer = current_time + seconds;
		}
		else
		{
			ptr_app->is_enabled = false;
			ptr_app->is_haveerrors = false;

			ptr_app->timer = 0;

			if (ptr_app->htimer)
			{
				DeleteTimerQueueTimer (config.htimer, ptr_app->htimer, nullptr);
				ptr_app->htimer = nullptr;
			}
		}
	}

	if (item_pos != INVALID_INT)
	{
		_r_fastlock_acquireshared (&lock_checkbox);

		_r_listview_setitem (hwnd, listview_id, item_pos, 0, nullptr, I_IMAGENONE, _app_getappgroup (app_hash, ptr_app));
		_r_listview_setitemcheck (hwnd, listview_id, item_pos, ptr_app->is_enabled);

		_r_fastlock_releaseshared (&lock_checkbox);
	}
}

void _app_timer_reset (HWND hwnd, PITEM_APP ptr_app)
{
	if (!ptr_app || !_app_istimeractive (ptr_app))
		return;

	ptr_app->is_enabled = false;
	ptr_app->is_haveerrors = false;

	ptr_app->timer = 0;

	if (ptr_app->htimer)
	{
		DeleteTimerQueueTimer (config.htimer, ptr_app->htimer, nullptr);
		ptr_app->htimer = nullptr;
	}

	const size_t app_hash = _r_str_hash (ptr_app->original_path); // note: be carefull (!)
	const INT listview_id = _app_getlistview_id (ptr_app->type);

	if (listview_id)
	{
		const INT item_pos = _app_getposition (hwnd, listview_id, app_hash);

		if (item_pos != INVALID_INT)
		{
			_r_fastlock_acquireshared (&lock_checkbox);
			_app_setappiteminfo (hwnd, listview_id, item_pos, app_hash, ptr_app);
			_r_fastlock_releaseshared (&lock_checkbox);
		}
	}
}

bool _app_istimeractive (const PITEM_APP ptr_app)
{
	return ptr_app->htimer || (ptr_app->timer && (ptr_app->timer > _r_unixtime_now ()));
}

bool _app_istimersactive ()
{
	_r_fastlock_acquireshared (&lock_access);

	for (auto &p : apps)
	{
		PR_OBJECT ptr_app_object = _r_obj_reference (p.second);

		if (!ptr_app_object)
			continue;

		PITEM_APP ptr_app = (PITEM_APP)ptr_app_object->pdata;

		if (ptr_app && _app_istimeractive (ptr_app))
		{
			_r_obj_dereference (ptr_app_object);
			_r_fastlock_releaseshared (&lock_access);

			return true;
		}

		_r_obj_dereference (ptr_app_object);
	}

	_r_fastlock_releaseshared (&lock_access);

	return false;
}

void CALLBACK _app_timer_callback (PVOID lparam, BOOLEAN)
{
	const HWND hwnd = app.GetHWND ();
	const size_t app_hash = (size_t)lparam;

	_r_fastlock_acquireshared (&lock_access);
	PR_OBJECT ptr_app_object = _app_getappitem (app_hash);
	_r_fastlock_releaseshared (&lock_access);

	if (!ptr_app_object)
		return;

	PITEM_APP ptr_app = (PITEM_APP)ptr_app_object->pdata;

	if (!ptr_app)
	{
		_r_obj_dereference (ptr_app_object);
		return;
	}

	OBJECTS_VEC rules;
	rules.push_back (ptr_app_object);

	_app_timer_reset (hwnd, ptr_app);
	_wfp_create3filters (_wfp_getenginehandle (), rules, __LINE__);

	_r_obj_dereference (ptr_app_object);

	const INT listview_id = _app_gettab_id (hwnd);

	_app_listviewsort (hwnd, listview_id);
	_app_refreshstatus (hwnd, listview_id);

	_app_profile_save ();

	_r_listview_redraw (hwnd, listview_id);

	if (app.ConfigGet (L"IsNotificationsTimer", true).AsBool ())
		_r_tray_popup (hwnd, UID, NIIF_INFO | (app.ConfigGet (L"IsNotificationsSound", true).AsBool () ? 0 : NIIF_NOSOUND), APP_NAME, _r_fmt (app.LocaleString (IDS_STATUS_TIMER_DONE, nullptr), ptr_app->display_name));
}
