#include <hobbes/storage.H>
#include <stdlib.h>

DEFINE_STORAGE_GROUP(
  Orders,
  3000,
  hobbes::storage::Reliable,    /* we _must_ track all orders */
  hobbes::storage::ManualCommit /* we need to correlate all events in an order */
);

int main() {
  while (true) {
    // a customer enters the store
    const char* names[] = { "Harv", "Beatrice", "Pat" };
    const char* name    = names[rand() % 3];

    HSTORE(Orders, customerEntered, name);

    // he/she orders a cheeseburger
    const char* products[] = { "BBQ Attack", "Cheese Quake", "Bacon Salad" };
    const char* product    = products[rand() % 3];

    HSTORE(Orders, productOrdered, product);

    // perhaps he/she decides to add a drink
    if (rand() % 5 == 0) {
      const char* drinks[] = { "Soda", "Lemonade", "Water" };
      const char* drink    = drinks[rand() % 3];

      HSTORE(Orders, drinkOrdered, drink);
    }

    // sometimes the customer has a change of heart, else they pay up and go
    if (rand() % 10 == 0) {
      HSTORE(Orders, orderCanceled);
    } else {
      HSTORE(Orders, paymentReceived, 10.0 * ((double)(rand() % 100)) / 100.0);
    }

    // now that the order is finished, end the transaction
    Orders.commit();
  }
  return 0;
}
