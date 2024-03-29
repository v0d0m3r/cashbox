#include "Atm_machine.hpp"
#include "Bank_machine.hpp"
#include "Interface_machine.hpp"

// Listing C.10 The driving code
int main()
try {
  bank_machine bank;
  interface_machine interface_hardware;
  atm machine(bank.get_sender(), interface_hardware.get_sender());
  std::thread bank_thread(&bank_machine::run, &bank);
  std::thread if_thread(&interface_machine::run, &interface_hardware);
  std::thread atm_thread(&atm::run, &machine);
  Messaging::Sender atmqueue(machine.get_sender());
  bool quit_pressed=false;
  constexpr auto how_much_to_withdraw{50};
  const auto which_card_we_inserted{std::string{"acc1234"}};
  while (!quit_pressed)
  {
    const auto inputed{static_cast<char>(getchar())};
    switch (inputed)
    {
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      atmqueue.send(digit_pressed(inputed));
      break;
    case 'b':
      atmqueue.send(balance_pressed());
      break;
    case 'w':
      atmqueue.send(withdraw_pressed(how_much_to_withdraw));
      break;
    case 'c':
      atmqueue.send(cancel_pressed());
      break;
    case 'q':
      quit_pressed=true;
      break;
    case 'i':
    case 'I':
      atmqueue.send(card_inserted(which_card_we_inserted));
      break;
    default:
      break;
    }
  }
  bank.done();
  machine.done();
  interface_hardware.done();
  atm_thread.join();
  bank_thread.join();
  if_thread.join();
  return 0;
}
catch (const std::exception&) {

}
