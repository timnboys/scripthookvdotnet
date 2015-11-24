/**
 * Copyright (C) 2015 Crosire
 *
 * This software is  provided 'as-is', without any express  or implied  warranty. In no event will the
 * authors be held liable for any damages arising from the use of this software.
 * Permission  is granted  to anyone  to use  this software  for  any  purpose,  including  commercial
 * applications, and to alter it and redistribute it freely, subject to the following restrictions:
 *
 *   1. The origin of this software must not be misrepresented; you must not claim that you  wrote the
 *      original  software. If you use this  software  in a product, an  acknowledgment in the product
 *      documentation would be appreciated but is not required.
 *   2. Altered source versions must  be plainly  marked as such, and  must not be  misrepresented  as
 *      being the original software.
 *   3. This notice may not be removed or altered from any source distribution.
 */

#include "ScriptDomain.hpp"

#include "Menu.hpp"
#include "Settings.hpp"

namespace GTA
{
	using namespace System;
	using namespace System::Threading;
	using namespace System::Windows::Forms;
	using namespace System::Collections::Concurrent;

	extern void HandleUnhandledException(Object ^sender, UnhandledExceptionEventArgs ^args);

	Script::Script() : mInterval(0), mRunning(false), mFilename(ScriptDomain::CurrentDomain->LookupScriptFilename(this)), mScriptDomain(ScriptDomain::CurrentDomain), mWaitEvent(gcnew AutoResetEvent(false)), mContinueEvent(gcnew AutoResetEvent(false)), mKeyboardEvents(gcnew ConcurrentQueue<Tuple<bool, KeyEventArgs ^> ^>())
	{
	}
	Script::~Script()
	{
	}

	Viewport ^Script::View::get()
	{
		if (Object::ReferenceEquals(this->mViewport, nullptr))
		{
			this->mViewport = gcnew GTA::Viewport();

			Tick += gcnew EventHandler(this, &Script::HandleViewportDraw);
			KeyUp += gcnew KeyEventHandler(this, &Script::HandleViewportInput);
		}

		return this->mViewport;
	}
	ScriptSettings ^Script::Settings::get()
	{
		if (Object::ReferenceEquals(this->mSettings, nullptr))
		{
			String ^path = IO::Path::ChangeExtension(this->mFilename, ".ini");

			if (IO::File::Exists(path))
			{
				this->mSettings = ScriptSettings::Load(path);
			}
			else
			{
				this->mSettings = gcnew ScriptSettings(path);
			}
		}

		return this->mSettings;
	}
	int Script::Interval::get()
	{
		return this->mInterval;
	}
	void Script::Interval::set(int value)
	{
		if (value < 0)
		{
			value = 0;
		}

		this->mInterval = value;
	}

	void Script::Abort()
	{
		this->mWaitEvent->Set();

		this->mScriptDomain->AbortScript(this);
	}
	static void Yield() {
		Script::Wait(0);
	}
	void Script::Wait(int ms)
	{
		Script ^script = ScriptDomain::ExecutingScript;

		if (Object::ReferenceEquals(script, nullptr) || !script->mRunning)
		{
			throw gcnew InvalidOperationException("Illegal call to 'Script.Wait()' outside main loop!");
		}

		const DateTime resume = DateTime::Now + TimeSpan::FromMilliseconds(ms);

		do
		{
			script->mWaitEvent->Set();
			script->mContinueEvent->WaitOne();
		}
		while (DateTime::Now < resume);
	}
	 /*void Yield()
	{
		
	}*/

	void Script::MainLoop()
	{
		// Wait for domain to run scripts
		this->mContinueEvent->WaitOne();

		// Run main loop
		while (this->mRunning)
		{
			Tuple<bool, KeyEventArgs ^> ^keyevent = nullptr;

			// Process events
			while (this->mKeyboardEvents->TryDequeue(keyevent))
			{
				try
				{
					if (keyevent->Item1)
					{
						KeyDown(this, keyevent->Item2);
					}
					else
					{
						KeyUp(this, keyevent->Item2);
					}
				}
				catch (Exception ^ex)
				{
					HandleUnhandledException(this, gcnew UnhandledExceptionEventArgs(ex, false));
					break;
				}
			}

			try
			{
				Tick(this, EventArgs::Empty);
			}
			catch (Exception ^ex)
			{
				HandleUnhandledException(this, gcnew UnhandledExceptionEventArgs(ex, true));

				Abort();
				break;
			}

			// Yield execution to next tick
			Wait(this->mInterval);
		}
	}
	void Script::HandleViewportDraw(Object ^sender, EventArgs ^e)
	{
		this->mViewport->Draw();
	}
	void Script::HandleViewportInput(Object ^sender, KeyEventArgs ^e)
	{
		if (e->KeyCode == ActivateKey) this->mViewport->HandleActivate();
		else if (e->KeyCode == BackKey) this->mViewport->HandleBack();
		else if (e->KeyCode == LeftKey) this->mViewport->HandleChangeItem(false);
		else if (e->KeyCode == RightKey) this->mViewport->HandleChangeItem(true);
		else if (e->KeyCode == UpKey) this->mViewport->HandleChangeSelection(false);
		else if (e->KeyCode == DownKey) this->mViewport->HandleChangeSelection(true);
	}
}