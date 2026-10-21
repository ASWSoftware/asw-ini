/* **************************************************************************
UT_Helper_Threads.cpp
Author: Anthony S. West - ASW Software

See header for info.

Copyright 2026 ASW Software

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    https://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

************************************************************************** */

//---------------------------------------------------------------------------
// Module header
#include "UT_Helper_Threads.h"
//---------------------------------------------------------------------------
#include <exception>
#include <thread>
//---------------------------------------------------------------------------

namespace ASWUnitTests
{

namespace
{

// The Id of the next TLifeId
std::atomic<std::uint32_t> g_NextLifeId{ 1 };

} // namespace

//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
ASWIni::TASWIniLogCallback CountingCallback(std::atomic<int>& count)
{
    TLifeId const life;
    auto const counter = [&count, life](ASWIni::TASWIniMessage const& /*message*/)
        {
            std::uint32_t const id = life.Id;
            std::this_thread::yield();
            if (id != 0 && life.Id == id)
                ++count;
        };

    return counter;
}
//---------------------------------------------------------------------------
void MaybeYield(std::mt19937& generator)
{
    if (generator() % 4 == 0)
        std::this_thread::yield();
}
//---------------------------------------------------------------------------
void RunTogether(std::vector<std::function<void()> > const& work, TThreadFailures& failures)
{
    std::atomic<bool> start{ false };
    auto const run = [&start, &failures](std::function<void()> const& function)
        {
            while (!start)
                std::this_thread::yield();

            try
            {
                function();
            }
            catch (std::exception const& e)
            {
                failures.Add(std::string("a thread threw: ") + e.what());
            }
            catch (...)
            {
                failures.Add("a thread threw an exception that isn't a std::exception");
            }
        };

    std::vector<std::thread> threads;
    try
    {
        for (std::function<void()> const& function : work)
            threads.emplace_back(run, std::cref(function));
    }
    catch (std::exception const& e)
    {
        failures.Add(std::string("a thread couldn't be started: ") + e.what());
    }

    start = true;
    for (std::thread& thread : threads)
        thread.join();
}
//---------------------------------------------------------------------------
std::string WriterKey(std::size_t index)
{
    return "Writer" + std::to_string(index);
}
//---------------------------------------------------------------------------


/////////////////////////////////////////////////////////////////////////////
// TLifeId
/////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------
TLifeId::TLifeId() noexcept
    : Id(g_NextLifeId++)
{
}
//---------------------------------------------------------------------------
TLifeId::TLifeId(TLifeId const& /*other*/) noexcept
    : Id(g_NextLifeId++)
{
}
//---------------------------------------------------------------------------
TLifeId::~TLifeId()
{
    Id = 0;
}
//---------------------------------------------------------------------------


/////////////////////////////////////////////////////////////////////////////
// TThreadFailures
/////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------
void TThreadFailures::Add(std::string const& problem)
{
    std::lock_guard<std::mutex> lock(m_Mutex);
    if (m_Count++ == 0)
        m_First = problem;
}
//---------------------------------------------------------------------------
std::string TThreadFailures::Summary() const
{
    std::lock_guard<std::mutex> lock(m_Mutex);
    if (m_Count == 0)
        return std::string();

    return std::to_string(m_Count) + " problem(s), with the threads seeded from " + std::to_string(ThreadsSeed) +
        "; the first: " + m_First;
}
//---------------------------------------------------------------------------

} // namespace ASWUnitTests
